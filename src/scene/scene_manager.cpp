#include "scene/scene_manager.h"

#include <memory>
#include <string_view>

#include "common/assert.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/manager.h"
#include "core/script.h"
#include "core/script_interfaces.h"
#include "input/input_handler.h"
#include "math/hash.h"
#include "renderer/render_data.h"
#include "renderer/renderer.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "scene/scene_transition.h"
#include "tweens/tween.h"

namespace ptgn::impl {

void SceneManager::UnloadImpl(std::size_t scene_key) {
	auto scene{ GetScene(scene_key) };
	if (scene) {
		scene.Get<SceneComponent>().scene->Add(Scene::Action::Unload);
	}
}

void SceneManager::EnterScene(std::size_t scene_key) {
	auto scene{ GetScene(scene_key) };
	PTGN_ASSERT(scene, "Cannot initialize a scene unless it has been loaded first");
	scene.Get<SceneComponent>().scene->Add(Scene::Action::Enter);
}

void SceneManager::EnterImpl(std::size_t scene_key) {
	if (HasActiveScene(scene_key)) {
		return;
	}
	auto scene{ GetScene(scene_key) };
	PTGN_ASSERT(scene, "Cannot enter scene unless it has been loaded first");

	bool first_scene{ GetActiveSceneCount() == 0 };

	EnterScene(scene_key);

	if (first_scene) {
		// First active scene, aka the starting scene. Enter the game loop.
		game.MainLoop();
	}
}

void SceneManager::ExitAll() {
	for (auto e : scenes_.Entities()) {
		const auto& sc{ e.Get<SceneComponent>() };
		if (sc.scene->active_) {
			sc.scene->Add(Scene::Action::Exit);
		}
	}
}

void SceneManager::UnloadAllScenes() {
	for (auto e : scenes_.Entities()) {
		const auto& sc{ e.Get<SceneComponent>() };
		sc.scene->Add(Scene::Action::Unload);
	}
}

void SceneManager::ExitImpl(std::size_t scene_key) {
	auto scene{ GetScene(scene_key) };
	if (!scene) {
		return;
	}
	auto& sc{ scene.Get<SceneComponent>() };
	if (!sc.scene->active_) {
		return;
	}
	sc.scene->Add(Scene::Action::Exit);
}

void SceneManager::TransitionImpl(
	std::size_t from_scene_key, std::size_t to_scene_key, const SceneTransition& transition
) {
	if (transition == SceneTransition{}) {
		ExitImpl(from_scene_key);
		EnterImpl(to_scene_key);
		return;
	}

	if (HasActiveScene(to_scene_key)) {
		return;
	}
	auto from{ GetScene(from_scene_key).Get<SceneComponent>().scene.get() };
	auto to{ GetScene(from_scene_key).Get<SceneComponent>().scene.get() };
	transition.Start(false, from_scene_key, to_scene_key, from);
	transition.Start(true, to_scene_key, from_scene_key, to);
}

void SceneManager::SwitchActiveScenesImpl(std::size_t scene1, std::size_t scene2) {
	PTGN_ASSERT(
		HasActiveScene(scene1),
		"Cannot switch scene which does not exist in the active scene vector"
	);
	PTGN_ASSERT(
		HasActiveScene(scene2),
		"Cannot switch scene which does not exist in the active scene vector"
	);
	auto s1{ GetActiveScene(scene1) };
	auto s2{ GetActiveScene(scene2) };
	auto& sc1{ s1.Get<SceneComponent>() };
	auto& sc2{ s2.Get<SceneComponent>() };
	std::swap(sc1.scene, sc2.scene);
}

void SceneManager::Reset() {
	UnloadAllScenes();
	HandleSceneEvents();
	scenes_.Reset();
}

void SceneManager::Shutdown() {
	Reset();
}

void SceneManager::ReEnter(std::size_t scene_key) {
	auto scene{ GetScene(scene_key) };
	PTGN_ASSERT(scene, "Cannot re-enter a scene that is not loaded in the scene manager");
	auto& sc{ scene.Get<SceneComponent>() };
	PTGN_ASSERT(sc.scene->active_, "Cannot re-enter a scene that is not active");
	sc.scene->Add(Scene::Action::Enter);
}

std::size_t SceneManager::GetInternalKey(std::string_view key) {
	return Hash(key);
}

/*
void SceneManager::ClearSceneTargets() {
	for (auto [s, sc] : scenes_.EntitiesWith<SceneComponent>()) {
		if (sc.scene->active_) {
			sc.scene->ClearTarget();
		}
	}
}
*/

void SceneManager::Update() {
	auto& render_data{ game.renderer.GetRenderData() };

	game.input.Update();

	// TODO: Figure out a better way to do non-scene events / scripts.

	bool invoke_actions{ render_data.logical_resolution_changed_ ||
						 render_data.physical_resolution_changed_ };

	const auto invoke_resolution_events = [&](Manager& manager) {
		manager.Refresh();

		if (render_data.logical_resolution_changed_) {
			for (auto [e, scripts] : manager.EntitiesWith<Scripts>()) {
				scripts.AddAction(&LogicalResolutionScript::OnLogicalResolutionChanged);
			}
		}
		if (render_data.physical_resolution_changed_) {
			for (auto [e, scripts] : manager.EntitiesWith<Scripts>()) {
				scripts.AddAction(&PhysicalResolutionScript::OnPhysicalResolutionChanged);
			}
		}
		if (invoke_actions) {
			for (auto [e, scripts] : manager.EntitiesWith<Scripts>()) {
				scripts.InvokeActions();
			}
		}

		manager.Refresh();
	};

	game.input.InvokeInputEvents(render_data.render_manager);
	invoke_resolution_events(render_data.render_manager);

	Tween::Update(render_data.render_manager, game.dt());

	for (auto [s, sc] : scenes_.EntitiesWith<SceneComponent>()) {
		PTGN_ASSERT(sc.scene != nullptr);
		invoke_resolution_events((*sc.scene).cameras_);
		invoke_resolution_events(*sc.scene);
		if (sc.scene->active_) {
			sc.scene->InternalUpdate();
		}
	}

	render_data.DrawScreenTarget();

	render_data.logical_resolution_changed_	 = false;
	render_data.physical_resolution_changed_ = false;
}

void SceneManager::HandleSceneEvents() {
	for (auto [e, sc] : scenes_.EntitiesWith<SceneComponent>()) {
		while (!e.Get<SceneComponent>().scene->actions_.empty()) {
			auto action{ e.Get<SceneComponent>().scene->actions_.begin() };
			switch (*action) {
				case Scene::Action::Enter:
					if (e.Get<SceneComponent>().scene->active_) {
						e.Get<SceneComponent>().scene->InternalExit();
					}
					// Reference may get invalidated if Exit adds a scene to the scene manager.
					e.Get<SceneComponent>().scene->InternalEnter();
					break;
				case Scene::Action::Exit: e.Get<SceneComponent>().scene->InternalExit(); break;
				case Scene::Action::Unload:
					if (e.Get<SceneComponent>().scene->active_) {
						e.Get<SceneComponent>().scene->InternalExit();
					}
					e.Destroy();
					break;
			}
			// Reference may get invalidated if an Enter or Exit adds a scene to the scene manager.
			e.Get<SceneComponent>().scene->actions_.erase(action);
		}
	}

	scenes_.Refresh();
}

std::size_t SceneManager::GetActiveSceneCount() const {
	std::size_t active_scene_count{ 0 };
	for (auto e : scenes_.Entities()) {
		const auto& sc{ e.Get<SceneComponent>() };
		if (sc.scene->active_) {
			active_scene_count++;
		}
	}
	return active_scene_count;
}

bool SceneManager::HasActiveScene(std::size_t scene_key) const {
	return GetActiveScene(scene_key).operator bool();
}

bool SceneManager::HasScene(std::size_t scene_key) const {
	return GetScene(scene_key).operator bool();
}

Entity SceneManager::GetActiveScene(std::size_t scene_key) const {
	for (auto e : scenes_.Entities()) {
		const auto& sc{ e.Get<SceneComponent>() };
		if (sc.scene->active_ && sc.scene->key_ == scene_key) {
			return e;
		}
	}
	return {};
}

Entity SceneManager::GetScene(std::size_t scene_key) const {
	for (auto e : scenes_.Entities()) {
		const auto& sc{ e.Get<SceneComponent>() };
		if (sc.scene->key_ == scene_key) {
			return e;
		}
	}
	return {};
}

} // namespace ptgn::impl