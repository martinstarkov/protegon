#include "scene/scene_manager.h"

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
	bool first_scene{ current_scene_ == nullptr };
	Get(scene_key)->Add(Scene::Action::Enter);

	// TODO: Fix scene transitions:
	// transition.Start(false, from_scene_key, to_scene_key, Get(from_scene_key));
	// transition.Start(true, to_scene_key, from_scene_key, Get(to_scene_key));

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
	return *current_scene_;
}

const Scene& SceneManager::GetCurrent() const {
	PTGN_ASSERT(HasCurrent(), "There is no current scene");
	return *current_scene_;
}

bool SceneManager::HasCurrent() const {
	return current_scene_ != nullptr;
}

void SceneManager::Reset() {
	UnloadAll();
	HandleSceneEvents();
	current_scene_ = nullptr;
	MapManager::Reset();
}

void SceneManager::Shutdown() {
	Reset();
}

void SceneManager::EnterScene(const std::shared_ptr<Scene>& scene) {
	game.input.ResetKeyStates();
	game.input.ResetMouseStates();
	if (HasCurrent()) {
		current_scene_->Exit();
	}
	current_scene_ = scene;
	current_scene_->Enter();
}

void SceneManager::Update() {
	if (current_scene_ == nullptr || !current_scene_->actions_.empty()) {
		return;
	}
	current_scene_->Update();
}

void SceneManager::HandleSceneEvents() {
	auto& map{ GetMap() };
	for (auto it{ map.begin() }; it != map.end();) {
		// Intentional reference counter increment to maintain scene during scene function calls.
		auto [key, scene] = *it;

		bool unload{ false };

		while (!scene->actions_.empty()) {
			auto action{ scene->actions_.begin() };
			switch (*action) {
				case Scene::Action::Enter:
					// Input is reset to ensure no previously pressed keys are considered held
					// between scenes.
					EnterScene(scene);
					break;
				case Scene::Action::Unload:
					if (current_scene_ == scene) {
						current_scene_->Exit();
					}
					unload = true;
					break;
				default: PTGN_ERROR("Unrecognized scene action");
			}
			scene->actions_.erase(action);
		}

		if (unload) {
			it = map.erase(it);
		} else {
			++it;
		}
	}
}

} // namespace ptgn::impl