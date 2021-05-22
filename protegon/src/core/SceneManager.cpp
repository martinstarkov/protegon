#include "SceneManager.h"

#include <algorithm>

namespace engine {

std::unordered_set<std::size_t> SceneManager::queued_unload_scenes_;
std::unordered_map<std::size_t, Scene*> SceneManager::loaded_scenes_;
std::unordered_map<std::size_t, std::size_t> SceneManager::display_scenes_;

void SceneManager::EnterScene(const char* scene_key, std::size_t display_index) {
	auto key{ Hasher::HashCString(scene_key) };
	auto display_it{ display_scenes_.find(display_index) };
	bool replacing{ display_it != display_scenes_.end() };
	// Exit early if active scene is the scene to be replaced.
	if (replacing && display_it->second == key) return;
	auto scene_it{ loaded_scenes_.find(key) };
	assert(scene_it != loaded_scenes_.end() &&
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
		auto old_scene{ GetScene(display_it->second) };
		if (old_scene != nullptr) {
			old_scene->Exit();
		}
		display_it->second = key;
	} else {
		display_scenes_.emplace(display_index, key);
	}

}

void SceneManager::UnloadScene(const char* scene_key) {
	queued_unload_scenes_.emplace(Hasher::HashCString(scene_key));
}

void SceneManager::UnloadQueuedScenes() {
	for (auto scene_key : queued_unload_scenes_) {
		// Remove scene key from display map.
		for (auto it{ display_scenes_.begin() }; it != display_scenes_.end(); ) {
			if (it->second == scene_key) {
				it = display_scenes_.erase(it);
			} else {
				++it;
			}
		}
		// Remove loaded scene.
		auto it{ loaded_scenes_.find(scene_key) };
		if (it != loaded_scenes_.end()) {
			delete it->second;
			loaded_scenes_.erase(it);
		}
	}
	queued_unload_scenes_.clear();
}

}
