#include "scene/scene_manager.h"

#include <memory>
#include <string_view>

#include "core/game.h"
#include "ecs/ecs.h"
#include "math/hash.h"
#include "scene/scene.h"
#include "scene/scene_transition.h"

namespace ptgn::impl {

void SceneManager::UnloadImpl(std::size_t scene_key) {
	auto scene{ GetScene(scene_key) };
	if (scene != ecs::null) {
		scene.Get<SceneComponent>().scene->Add(Scene::Action::Unload);
	}
}

void SceneManager::EnterScene(std::size_t scene_key) {
	auto scene{ GetScene(scene_key) };
	PTGN_ASSERT(scene != ecs::null, "Cannot initialize a scene unless it has been loaded first");
	scene.Get<SceneComponent>().scene->Add(Scene::Action::Enter);
}

void SceneManager::EnterImpl(std::size_t scene_key) {
	if (HasActiveScene(scene_key)) {
		return;
	}
	auto scene{ GetScene(scene_key) };
	PTGN_ASSERT(scene != ecs::null, "Cannot enter scene unless it has been loaded first");

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
	if (scene == ecs::null) {
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
	for (auto [s, sc] : scenes_.EntitiesWith<SceneComponent>()) {
		if (sc.scene->active_ && sc.scene->actions_.empty()) {
			sc.scene->InternalUpdate();
		}
	}
}

void SceneManager::HandleSceneEvents() {
	for (auto [e, sc] : scenes_.EntitiesWith<SceneComponent>()) {
		auto& scene{ *sc.scene };

		while (!scene.actions_.empty()) {
			auto action{ scene.actions_.begin() };
			switch (*action) {
				case Scene::Action::Enter:
					if (scene.active_) {
						scene.InternalExit();
					}
					scene.InternalEnter();
					break;
				case Scene::Action::Exit: scene.InternalExit(); break;
				case Scene::Action::Unload:
					if (scene.active_) {
						scene.InternalExit();
					}
					scene.InternalUnload();
					e.Destroy();
					break;
			}
			scene.actions_.erase(action);
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
	return GetActiveScene(scene_key) != ecs::null;
}

bool SceneManager::HasScene(std::size_t scene_key) const {
	return GetScene(scene_key) != ecs::null;
}

ecs::Entity SceneManager::GetActiveScene(std::size_t scene_key) const {
	for (auto e : scenes_.Entities()) {
		const auto& sc{ e.Get<SceneComponent>() };
		if (sc.scene->active_ && sc.scene->key_ == scene_key) {
			return e;
		}
	}
	return ecs::null;
}

ecs::Entity SceneManager::GetScene(std::size_t scene_key) const {
	for (auto e : scenes_.Entities()) {
		const auto& sc{ e.Get<SceneComponent>() };
		if (sc.scene->key_ == scene_key) {
			return e;
		}
	}
	return ecs::null;
}

} // namespace ptgn::impl