#include "scene/scene_manager.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "common/assert.h"
#include "core/game.h"
#include "core/manager.h"
#include "core/script.h"
#include "core/script_interfaces.h"
#include "input/input_handler.h"
#include "renderer/render_data.h"
#include "renderer/renderer.h"
#include "scene/scene.h"
#include "scene/scene_key.h"
#include "tweens/tween.h"
#include "utility/span.h"

namespace ptgn::impl {

const Scene& SceneManager::GetCurrent() const {
	PTGN_ASSERT(current_, "Cannot get current scene when one has not been set");
	return *current_;
}

Scene& SceneManager::GetCurrent() {
	return const_cast<Scene&>(std::as_const(*this).GetCurrent());
}

bool SceneManager::Has(const SceneKey& scene_key) const {
	return GetImpl(scene_key) != nullptr;
}

bool SceneManager::IsActive(const SceneKey& scene_key) const {
	return VectorFindIf(active_scenes_, [scene_key](const auto& scene) {
		return scene->key_ == scene_key;
	});
}

std::shared_ptr<Scene> SceneManager::GetImpl(const SceneKey& scene_key) const {
	auto queued_it{ std::ranges::find_if(queued_scenes_, [scene_key](const auto& scene) {
		return scene->key_ == scene_key;
	}) };
	if (queued_it != queued_scenes_.end()) {
		return *queued_it;
	}
	auto it{ std::ranges::find_if(scenes_, [scene_key](const auto& scene) {
		return scene->key_ == scene_key;
	}) };
	if (it == scenes_.end()) {
		return nullptr;
	}
	return *it;
}

void SceneManager::Unload(const SceneKey& scene_key) {
	auto scene{ GetImpl(scene_key) };
	if (scene) {
		scene->state_ = Scene::State::Unloading;
	}
}

void SceneManager::Enter(const SceneKey& scene_key) {
	auto scene{ GetImpl(scene_key) };
	PTGN_ASSERT(scene, "Cannot enter a scene unless it has been loaded first");

	scene->state_ = Scene::State::Entering;

	if (active_scenes_.empty()) {
		// First active scene, aka the starting scene. Enter the game loop.
		game.MainLoop();
	}
}

void SceneManager::Exit(const SceneKey& scene_key) {
	auto scene{ GetImpl(scene_key) };

	if (!scene) {
		return;
	}

	if (!IsActive(scene_key)) {
		return;
	}

	scene->state_ = Scene::State::Exiting;
}

void SceneManager::Reset() {
	for (auto& scene : scenes_) {
		scene->state_ = Scene::State::Unloading;
	}
	HandleSceneEvents();
	scenes_		   = {};
	active_scenes_ = {};
	queued_scenes_ = {};
	current_	   = nullptr;
}

void SceneManager::Shutdown() {
	Reset();
}

void SceneManager::Update(Game& g) {
	HandleSceneEvents();

	if (active_scenes_.empty()) {
		return;
	}

	g.renderer.ClearScreen();

	auto& render_data{ g.renderer.GetRenderData() };

	g.input.Update();

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

	g.input.InvokeInputEvents(render_data.render_manager);
	invoke_resolution_events(render_data.render_manager);

	Tween::Update(render_data.render_manager, g.dt());

	auto scenes{ scenes_ };
	for (auto scene : scenes) {
		invoke_resolution_events(scene->cameras_);
		invoke_resolution_events(*scene);
	}

	auto active_scenes{ active_scenes_ };
	for (auto active_scene : active_scenes) {
		current_ = active_scene;
		active_scene->InternalUpdate();
		current_ = nullptr;
	}

	render_data.DrawScreenTarget();

	render_data.logical_resolution_changed_	 = false;
	render_data.physical_resolution_changed_ = false;

	g.renderer.PresentScreen();
}

void SceneManager::HandleSceneEvents() {
	for (const auto& queued_scene : queued_scenes_) {
		VectorReplaceOrEmplaceIf(
			scenes_,
			[&queued_scene](const auto& scene) { return scene->key_ == queued_scene->key_; },
			queued_scene
		);
	}
	queued_scenes_ = {};

	// If bool is true, erase scene from active vector.
	std::vector<std::pair<bool, std::shared_ptr<Scene>>> exit;
	// If bool is true, add scene to active vector.
	std::vector<std::pair<bool, std::shared_ptr<Scene>>> enter;
	std::vector<std::shared_ptr<Scene>> unload;

	for (auto scene : scenes_) {
		switch (scene->state_) {
			case Scene::State::Running:		break;
			case Scene::State::Constructed: break;
			case Scene::State::Paused:		break;
			case Scene::State::Sleeping:	break;
			case Scene::State::Entering:	{
				if (IsActive(scene->key_)) {
					exit.emplace_back(false, scene);
					enter.emplace_back(false, scene);
				} else {
					enter.emplace_back(true, scene);
				}
				scene->state_ = Scene::State::Running;
				break;
			}
			case Scene::State::Exiting:
				exit.emplace_back(true, scene);
				scene->state_ = Scene::State::Constructed;
				break;
			case Scene::State::Unloading: {
				if (IsActive(scene->key_)) {
					exit.emplace_back(true, scene);
				}
				unload.emplace_back(scene);
				break;
			}
		}
	}
	for (auto [erase_active, scene] : exit) {
		scene->InternalExit();
		if (erase_active) {
			VectorErase(active_scenes_, scene);
		}
	}
	for (auto [add_active, scene] : enter) {
		if (add_active) {
			active_scenes_.emplace_back(scene);
		}
		scene->InternalEnter();
	}
	for (auto scene : unload) {
		VectorErase(scenes_, scene);
	}
}

} // namespace ptgn::impl