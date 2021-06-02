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
	// Load a scene into the scene manager. Allows for constructor arguments.
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
	// Load a scene into the scene manager. Allows for constructor arguments.
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

	// May return nullptr if no active scene is set.
	template <typename TScene,
		type_traits::is_base_of_e<Scene, TScene> = true>
	static TScene* GetActiveScene() {
		return static_cast<TScene*>(GetInstance().active_scene_);
	}

	// Sets the scene to active.
	static void SetActiveScene(const char* scene_key);
	
	// Flags a scene to be unloaded after the game loop cycle.
	static void UnloadScene(const char* scene_key);
private:
	friend class Engine;
	friend class Singleton<SceneManager>;

	void LoadSceneImpl(const char* scene_key, Scene* scene);

	// Retrieves the given scene.
	Scene* GetScene(std::size_t key);

	void UpdateActiveSceneImpl();

	static void UpdateActiveScene();

	static void RenderActiveScene();

	static void UnloadQueuedScenes();

	SceneManager() = default;
	~SceneManager();

	// Element: scene_key
	std::unordered_set<std::size_t> unload_scenes_;

	Scene* queued_scene_{ nullptr };
	Scene* active_scene_{ nullptr };

	// Key: scene_key
	// Value: scene pointer
	std::unordered_map<std::size_t, Scene*> scenes_;
};

} // namespace ptgn