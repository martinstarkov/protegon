#include "SceneManager.h"

namespace engine {

void SceneManager::SetActiveScene(const char* scene_key, std::size_t display_index) {
	auto key{ Hasher::HashCString(scene_key) };
	auto scene{ GetScene(key) };
	scene->SetDisplayIndex(display_index);
	auto it{ display_scenes_.find(key) };
	if (it != display_scenes_.end()) {
		delete it->second;
		it->second = scene;
	} else {
		display_scenes_.emplace(key, scene);
	}

}

void SceneManager::UnloadScene(const char* scene_key) {

}

}
