#pragma once

#include <cstdlib> // std::size_t
#include <unordered_map> // std::unordered_map

#include "core/Scene.h"

#include "math/Hasher.h"

#include "utils/TypeTraits.h"

namespace engine {

class SceneManager {
private:
public:
	template <typename T,
		engine::type_traits::is_base_of<Scene, T>
	>
	static T& AddScene(const char* scene_key) {
		static_assert(std::is_default_constructible_v<T>,
					  "Cannot add scene to scene manager which is not default constructible");
		auto scene{ new T{} };
		auto key{ Hasher::HashCString(scene_key) };
		auto it{ loaded_scenes_.find(key) };
		if (it != loaded_scenes_.end()) {
			delete it->second;
			it->second = scene;
		} else {
			loaded_scenes_.emplace(key, scene);
		}
		return *scene;
	}

	template <typename T,
		engine::type_traits::is_base_of<Scene, T>
	>
	static T& GetScene(const char* scene_key) {
		return static_cast<T>(GetScene(Hasher::HashCString(scene_key)));
	}

	static void SetActiveScene(const char* scene_key, std::size_t display_index = 0);
	static void UnloadScene(const char* scene_key);
private:
	static Scene* GetScene(std::size_t scene_key) {
		auto it{ loaded_scenes_.find(scene_key) };
		assert(it != loaded_scenes_.end() &&
			   "Cannot retrieve scene which does not exist in scene manager");
		return it->second;
	}
	static void EnterActiveScenes() {
		for (auto& active_scene : loaded_scenes_) {
			if (!active_scene.second->entered_) {
				active_scene.second->Enter();
				active_scene.second->entered_ = true;
			}
		}
	}
	static void UpdateActiveScenes() {
		for (auto& active_scene : loaded_scenes_) {
			active_scene.second->Update();
		}
	}
	static void RenderActiveScenes() {
		for (auto& active_scene : loaded_scenes_) {
			active_scene.second->Render();
		}
	}
	// Key: scene_key
	// Value: scene pointer
	static std::unordered_map<std::size_t, Scene*> loaded_scenes_;
	// Key: display_index 
	// Value: scene_key
	static std::unordered_map<std::size_t, std::size_t> display_scenes_;
};

} // namespace engine