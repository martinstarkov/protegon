#pragma once

#include <cstdlib> // std::size_t
#include <unordered_map> // std::unordered_map
#include <unordered_set> // std::unordered_set
#include <vector> // std::vector

#include "core/Scene.h"
#include "math/Math.h"
#include "utils/Singleton.h"
#include "utils/TypeTraits.h"

namespace ptgn {

class SceneManager : public Singleton<SceneManager> {
private:
public:
	// Load a scene into the scene manager via default construction.
	// This constructs the scene but does not set it as active.
	template <typename T, 
		type_traits::is_default_constructible_e<T> = true,
		type_traits::is_base_of_e<Scene, T> = true>
	static T& LoadScene(const char* scene_key) {
		auto new_scene{ new T{} };
		auto& instance{ GetInstance() };
		instance.LoadSceneImpl(scene_key, new_scene);
		return *new_scene;
	}
	// Load a scene into the scene manager. 
	// Allows for scene constructor arguments to be passed.
	// This constructs the scene but does not set it as active.
	template <typename TScene, typename ...TArgs,
		type_traits::is_base_of_e<Scene, TScene> = true>
	static TScene& LoadScene(const char* scene_key, TArgs&&... constructor_args) {
		static_assert(std::is_constructible_v<TScene, TArgs...>,
					  "Cannot load and construct the scene from passed arguments");
		auto new_scene{ new TScene(std::forward<TArgs>(constructor_args)...) };
		auto& instance{ GetInstance() };
		instance.LoadSceneImpl(scene_key, new_scene);
		return *new_scene;
	}

	// Sets the scene to active.
	static void SetActiveScene(const char* scene_key);
	
	// Flags a scene to be unloaded after the game loop cycle.
	static void UnloadScene(const char* scene_key);
private:
	friend class Engine;
	friend class Singleton<SceneManager>;

	// May return nullptr if no active scene is set.
	template <typename TScene,
		type_traits::is_base_of_e<Scene, TScene> = true>
	static TScene* GetActiveScene() {
		return static_cast<TScene*>(GetInstance().active_scene_);
	}

	// Implementation of LoadScene.
	void LoadSceneImpl(const char* scene_key, Scene* scene);

	// Retrieves the scene with the given key, nullptr if no such scene exists.
	Scene* GetScene(std::size_t key);

	// Implementation of UpdateActiveScene, done to avoid typing "instance." in front of every term.
	void UpdateActiveSceneImpl();

	// Transitions to the previously queued scene (sets it as active).
	// Then updates the currently active scene.
	static void UpdateActiveScene();

	// Renders the currently active scene.
	static void RenderActiveScene();

	// Unloads any scenes that have been flagged for unloading during the frame.
	static void UnloadQueuedScenes();

	SceneManager() = default;
	~SceneManager();

	// Element: scene_key
	std::unordered_set<std::size_t> unload_scenes_;

	// Scene which will be set as active after the current cycle.
	Scene* queued_scene_{ nullptr };
	
	// Currently active scene.
	Scene* active_scene_{ nullptr };

	// Key: scene_key
	// Value: scene pointer
	std::unordered_map<std::size_t, Scene*> scenes_;
};

} // namespace ptgn