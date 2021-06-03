#pragma once

#include <cstdlib> // std::size_t
#include <vector> // std::vector
#include <utility> // std::forward

#include "core/Scene.h"
#include "math/Math.h"
#include "utils/Singleton.h"
#include "utils/TypeTraits.h"

namespace ptgn {

class SceneManager : public Singleton<SceneManager> {
private:
public:
	template <typename TScene, typename ...TArgs,
		type_traits::is_base_of_e<Scene, TScene> = true>
	static void LoadScene(const char* scene_key, TArgs&&... args) {
		GetInstance().LoadSceneImpl(math::Hash(scene_key), new TScene(std::forward<TArgs>(args)...));
	}

	template <typename TScene,
		type_traits::is_default_constructible_e<TScene> = true,
		type_traits::is_base_of_e<Scene, TScene> = true>
	static void LoadScene(const char* scene_key) {
		GetInstance().LoadSceneImpl(math::Hash(scene_key), new TScene{});
	}

	static void SetActiveScene(const char* scene_key);

	static bool HasScene(const char* scene_key);

	static void UnloadScene(const char* scene_key);
private:
	friend class Engine;
	friend class Singleton<SceneManager>;

	static void UpdateActiveScene();
	static void RenderActiveScene();
	static void UnloadFlaggedScenes();

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