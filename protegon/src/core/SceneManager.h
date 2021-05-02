#pragma once

#include <cstdlib> // std::size_t
#include <unordered_map> // std::unordered_map
#include <unordered_set> // std::unordered_set

#include "core/Scene.h"

#include "math/Hasher.h"

#include "utils/TypeTraits.h"

namespace engine {

class SceneManager {
private:
public:
	// Load a scene into the scene manager. 
	// This allocates the scene and associated memory,
	// but does not set it as active.
	template <typename T,
		engine::type_traits::is_base_of<Scene, T> = true
	>
	static T& LoadScene(const char* scene_key) {
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

	// Retrieves and casts the active scene to a given type.
	template <typename T,
		engine::type_traits::is_base_of<Scene, T> = true
	>
	static T& GetScene(const char* scene_key) {
		auto key{ Hasher::HashCString(scene_key) };
		auto scene{ GetScene(scene_key) };
		assert(scene != nullptr &&
			   "Cannot retrieve scene which does not exist in scene manager");
		return static_cast<T>(it->second);
	}
	// Sets the scene to active for the given display_index.
	static void SetActiveScene(const char* scene_key, std::size_t display_index = 0);
	// Flags a scene to be unloaded after the game loop cycle.
	static void UnloadScene(const char* scene_key);

	// TODO: TEMPORARY, MOVE TO PRIVATE:

	static void EnterActiveScenes() {
		for (auto& pair : display_scenes_) {
			auto scene{ GetScene(pair.second) };
			assert(scene != nullptr && 
				   "Cannot enter active scene which has been deleted");
			if (!scene->entered_) {
				scene->Enter();
				scene->entered_ = true;
			}
		}
	}
	static void UpdateActiveScenes() {
		for (auto& pair : display_scenes_) {
			auto scene{ GetScene(pair.second) };
			assert(scene != nullptr && 
				   "Cannot update active scene which has been deleted");
			if (scene->entered_) {
				scene->Update();
			}
		}
	}
	static void RenderActiveScenes() {
		for (auto& pair : display_scenes_) {
			auto scene{ GetScene(pair.second) };
			assert(scene != nullptr && 
				   "Cannot render active scene which has been deleted");
			if (scene->entered_) {
				scene->Render();
			}
		}
	}
	static void UnloadQueuedScenes();

private:
	static Scene* GetScene(std::size_t scene_key) {
		auto it{ loaded_scenes_.find(scene_key) };
		if (it != loaded_scenes_.end()) {
			return it->second;
		}
		return nullptr;
	}
	// Element: scene_key
	static std::unordered_set<std::size_t> queued_unload_scenes_;
	// Key: scene_key
	// Value: scene pointer
	static std::unordered_map<std::size_t, Scene*> loaded_scenes_;
	// Key: display_index
	// Value: scene_key
	static std::unordered_map<std::size_t, std::size_t> display_scenes_;
};

} // namespace engine