#pragma once

#include <cstdlib> // std::size_t
#include <vector> // std::vector
#include <utility> // std::forward
#include <type_traits> // std::enable_if_t

#include "scene/Scene.h"
#include "math/Math.h"
#include "scene/Camera.h"

namespace ptgn {

class SceneManager {
private:
public:
	template <typename TScene, typename ...TArgs,
		std::enable_if_t<std::is_base_of_v<Scene, TScene>, bool> = true>
	static void LoadScene(const char* scene_key, TArgs&&... args) {
		static_assert(std::is_constructible_v<TScene, TArgs...>,
					  "Cannot construct scene from passed arguments");
		GetInstance().LoadSceneImpl(math::Hash(scene_key), new TScene(std::forward<TArgs>(args)...));
	}

	template <typename TScene,
		std::enable_if_t<std::is_default_constructible_v<TScene>, bool> = true,
		std::enable_if_t<std::is_base_of_v<Scene, TScene>, bool> = true>
	static void LoadScene(const char* scene_key) {
		GetInstance().LoadSceneImpl(math::Hash(scene_key), new TScene{});
	}

	static void SetActiveScene(const char* scene_key);

	static bool HasScene(const char* scene_key);

	static void UnloadScene(const char* scene_key);

	//static Camera& GetActiveCamera();
private:
	friend class Engine;

	//static Scene& GetActiveScene();
	static void RenderActiveScene();
	static void UnloadFlaggedScenes();
	static void UpdateActiveScene();

	std::vector<Scene*>::iterator GetSceneImpl(std::size_t);
	void LoadSceneImpl(std::size_t scene_key, Scene* scene);
	void UnloadSceneImpl(std::size_t scene_key);
	void SetActiveSceneImpl(std::size_t scene_key);

	SceneManager() = default;
	~SceneManager();

	Scene* previous_scene_{ nullptr };
	// Active scene is first element.
	std::vector<Scene*> scenes_;
};

} // namespace ptgn