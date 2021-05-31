#include "SceneManager.h"

#include <algorithm>

namespace ptgn {

void SceneManager::EnterScene(const char* scene_key) {
	auto& instance{ GetInstance() };
	auto key{ Hasher::HashCString(scene_key) };
	auto active_it{ instance.active_scenes_.find(key) };
	bool replacing{ active_it != instance.active_scenes_.end() };
	// Exit early if active scene is the scene to be replaced.
	if (replacing && *active_it == key) return;
	auto scene_it{ instance.loaded_scenes_.find(key) };
	assert(scene_it != instance.loaded_scenes_.end() &&
		   "Cannot set scene to active if it has not been added to the scene manager or has been unloaded");
	auto scene{ scene_it->second };
	assert(scene != nullptr &&
		   "Cannot set active scene to invalid scene pointer");
	if (!scene->entered_) {
		scene->Enter();
		scene->entered_ = true;
	}
	if (replacing) {
		// Exit old scene.
		auto old_scene{ instance.GetScene(*active_it) };
		if (old_scene != nullptr) {
			old_scene->Exit();
		}
	}
	instance.active_scenes_.emplace(key);
}

void SceneManager::UnloadScene(const char* scene_key) {
	auto& instance{ GetInstance() };
	instance.queued_unload_scenes_.emplace(Hasher::HashCString(scene_key));
}

void SceneManager::UnloadQueuedScenes() {
	auto& instance{ GetInstance() };
	for (auto scene_key : instance.queued_unload_scenes_) {
		// Remove scene key from display map.
		for (auto it{ instance.active_scenes_.begin() }; it != instance.active_scenes_.end(); ) {
			if (*it == scene_key) {
				it = instance.active_scenes_.erase(it);
			} else {
				++it;
			}
		}
		// Remove loaded scene.
		auto it{ instance.loaded_scenes_.find(scene_key) };
		if (it != instance.loaded_scenes_.end()) {
			delete it->second;
			instance.loaded_scenes_.erase(it);
		}
	}
	instance.queued_unload_scenes_.clear();
}

SceneManager::~SceneManager() {
	for (auto [key, scene] : loaded_scenes_) {
		delete scene;
	}
}

} // namespace ptgn