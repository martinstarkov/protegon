#include "scene/scene_manager.h"

#include <deque>
#include <list>
#include <memory>

#include "core/game.h"
#include "core/manager.h"
#include "event/input_handler.h"
#include "scene/scene.h"
#include "utility/debug.h"

namespace ptgn::impl {

void SceneManager::UnloadImpl(const InternalKey& scene_key) {
	if (Has(scene_key)) {
		Get(scene_key)->Add(Scene::Action::Unload);
	}
}

void SceneManager::EnterImpl(const InternalKey& scene_key, const SceneTransition& transition) {
	PTGN_ASSERT(
		Has(scene_key),
		"Cannot enter a scene which has not first been loaded into the scene manager"
	);
	bool first_scene{ current_scene_.second == nullptr };
	// Cannot transition to self.
	if (Get(scene_key) == current_scene_.second) {
		return;
	}

	if (transition.type_ == TransitionType::None) {
		Get(scene_key)->Add(Scene::Action::Enter);
		if (!transition.keep_loaded_ && current_scene_.second != nullptr) {
			current_scene_.second->Add(Scene::Action::Unload);
		}
	} else {
		bool empty_queue{ transition_queue_.empty() };
		// Cannot queue two transitions in a row to the same scene.
		if (empty_queue || transition_queue_.front().key != scene_key) {
			transition_queue_.emplace_back(transition, scene_key);
		}
		if (empty_queue) {
			bool from_valid_scene{ current_scene_.second != nullptr };
			transition.Start(from_valid_scene, current_scene_.first, scene_key);
		}
	}

	if (first_scene && !game.IsRunning()) {
		// First scene, aka the starting scene. Enter the game loop.
		game.MainLoop();
	}
}

void SceneManager::UnloadAll() {
	auto& map{ GetMap() };
	for (auto& [key, scene] : map) {
		scene->Add(Scene::Action::Unload);
	}
}

Scene& SceneManager::GetCurrent() {
	PTGN_ASSERT(HasCurrent(), "There is no current scene");
	return *current_scene_.second;
}

const Scene& SceneManager::GetCurrent() const {
	PTGN_ASSERT(HasCurrent(), "There is no current scene");
	return *current_scene_.second;
}

bool SceneManager::HasCurrent() const {
	return current_scene_.second != nullptr;
}

void SceneManager::Reset() {
	UnloadAll();
	HandleSceneEvents();
	PTGN_ASSERT(current_scene_.second == nullptr, "Failed to exit and unload current scene");
	transition_queue_ = {};
	MapManager::Reset();
}

void SceneManager::Shutdown() {
	Reset();
}

void SceneManager::EnterScene(InternalKey scene_key, const std::shared_ptr<Scene>& scene) {
	game.input.ResetKeyStates();
	game.input.ResetMouseStates();
	if (HasCurrent()) {
		current_scene_.second->Exit();
	}
	current_scene_.first  = scene_key;
	current_scene_.second = scene;
	current_scene_.second->Enter();
}

void SceneManager::Update() {
	if (current_scene_.second == nullptr) {
		return;
	}
	if (current_scene_.second->HasActions()) {
		return;
	}
	current_scene_.second->Update();
}

void SceneManager::HandleSceneEvents() {
	auto& map{ GetMap() };
	for (auto it{ map.begin() }; it != map.end();) {
		// Intentional reference counter increment to maintain scene during scene function
		// calls.
		auto [key, scene] = *it;

		bool unload{ false };

		while (scene->HasActions()) {
			auto action{ scene->actions_.begin() };
			switch (*action) {
				case Scene::Action::Enter:
					// Input is reset to ensure no previously pressed keys are considered held
					// between scenes.
					EnterScene(key, scene);
					break;
				case Scene::Action::Unload:
					if (current_scene_.second == scene) {
						current_scene_.second->Exit();
					}
					unload = true;
					break;
				default: PTGN_ERROR("Unrecognized scene action");
			}
			scene->Remove(*action);
		}

		if (unload && scene == current_scene_.second) {
			current_scene_.second = nullptr;
		}

		if (unload) {
			it = map.erase(it);
		} else {
			++it;
		}
	}
}

} // namespace ptgn::impl