#pragma once

#include <cstdlib> // std::size_t
#include <unordered_map> // std::unordered_map
#include <unordered_set> // std::unordered_set

#include "core/Scene.h"
#include "math/Hasher.h"
#include "utils/Singleton.h"
#include "utils/TypeTraits.h"

namespace engine {

class SceneManager : public Singleton<SceneManager> {
private:
public:
	// Load a scene into the scene manager. 
	// This allocates the scene and associated memory,
	// but does not set it as active.
	template <typename T,
		engine::type_traits::is_base_of_e<Scene, T> = true>
	static T& LoadScene(const char* scene_key) {
		static_assert(std::is_default_constructible_v<T>,
					  "Cannot add scene to scene manager which is not default constructible");
		auto& instance{ GetInstance() };
		auto scene{ new T{} };
		auto key{ Hasher::HashCString(scene_key) };
		auto it{ instance.loaded_scenes_.find(key) };
		if (it != instance.loaded_scenes_.end()) {
			delete it->second;
			it->second = scene;
		} else {
			instance.loaded_scenes_.emplace(key, scene);
		}
		return *scene;
	}

	// Retrieves and casts the active scene to a given type.
	template <typename T,
		engine::type_traits::is_base_of_e<Scene, T> = true>
	static T& GetScene(const char* scene_key) {
		auto& instance{ GetInstance() };
		auto key{ Hasher::HashCString(scene_key) };
		auto scene{ instance.GetScene(scene_key) };
		assert(scene != nullptr &&
			   "Cannot retrieve scene which does not exist in scene manager");
		return static_cast<T>(it->second);
	}

	// Sets the scene to active for the given display_index.
	static void EnterScene(const char* scene_key);
	
	// Flags a scene to be unloaded after the game loop cycle.
	static void UnloadScene(const char* scene_key);
	
private:
	friend class Engine;
	friend class Singleton<SceneManager>;

	static void EnterActiveScenes() {
		auto& instance{ GetInstance() };
		for (auto scene_id : instance.active_scenes_) {
			auto scene{ instance.GetScene(scene_id) };
			assert(scene != nullptr &&
				   "Cannot enter active scene which has been deleted");
			if (!scene->entered_) {
				scene->Enter();
				scene->entered_ = true;
			}
		}
	}
	static void UpdateActiveScenes() {
		auto& instance{ GetInstance() };
		for (auto scene_id : instance.active_scenes_) {
			auto scene{ instance.GetScene(scene_id) };
			assert(scene != nullptr &&
				   "Cannot update active scene which has been deleted");
			if (scene->entered_) {
				scene->Update();
			}
		}
	}
	static void RenderActiveScenes() {
		auto& instance{ GetInstance() };
		for (auto scene_id : instance.active_scenes_) {
			auto scene{ instance.GetScene(scene_id) };
			assert(scene != nullptr &&
				   "Cannot render active scene which has been deleted");
			if (scene->entered_) {
				scene->Render();
			}
		}
	}

	static void UnloadQueuedScenes();

	Scene* GetScene(std::size_t scene_key) {
		auto it{ loaded_scenes_.find(scene_key) };
		if (it != loaded_scenes_.end()) {
			return it->second;
		}
		return nullptr;
	}

	SceneManager() = default;
	~SceneManager();

	// Element: scene_key
	std::unordered_set<std::size_t> queued_unload_scenes_;
	// Key: scene_key
	// Value: scene pointer
	std::unordered_map<std::size_t, Scene*> loaded_scenes_;
	// Element: scene_key
	std::unordered_set<std::size_t> active_scenes_;
};

} // namespace engine