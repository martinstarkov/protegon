#pragma once

#include <cstdint>
#include <memory>
#include <type_traits>
#include <vector>

#include "core/manager.h"
#include "protegon/scene.h"
#include "utility/debug.h"
#include "utility/type_traits.h"

namespace ptgn {

using SceneKey = std::size_t;

namespace impl {

inline constexpr std::size_t start_scene_key{ 0 };

} // namespace impl

class SceneManager : public Manager<std::shared_ptr<Scene>> {
private:
	SceneManager()								 = default;
	~SceneManager() override					 = default;
	SceneManager(const SceneManager&)			 = delete;
	SceneManager(SceneManager&&)				 = default;
	SceneManager& operator=(const SceneManager&) = delete;
	SceneManager& operator=(SceneManager&&)		 = default;

public:
	template <
		typename T, typename... TArgs, tt::constructible<T, TArgs...> = true,
		tt::convertible<T*, Scene*> = true>
	std::shared_ptr<T> Load(SceneKey scene_key, TArgs&&... constructor_args) {
		PTGN_ASSERT(
			scene_key != impl::start_scene_key,
			"Cannot load scene with key == 0, it is reserved for the starting scene"
		);
		return std::static_pointer_cast<T>(Manager<std::shared_ptr<Scene>>::Load(
			scene_key, std::make_shared<T>(std::forward<TArgs>(constructor_args)...)
		));
	}

	template <typename TScene = Scene>
	[[nodiscard]] std::shared_ptr<TScene> Get(SceneKey scene_key) {
		static_assert(
			std::is_base_of_v<Scene, TScene> || std::is_same_v<TScene, Scene>,
			"Cannot cast retrieved scene to type which does not inherit from the Scene class"
		);
		return std::static_pointer_cast<TScene>(Manager<std::shared_ptr<Scene>>::Get(scene_key));
	}

	void Unload(std::size_t scene_key);

	void AddActive(std::size_t scene_key);
	void RemoveActive(std::size_t scene_key);
	[[nodiscard]] std::vector<std::shared_ptr<Scene>> GetActive();
	[[nodiscard]] Scene& GetTopActive();

private:
	void InitScene(std::size_t scene_key);

	friend class Game;

	template <typename TStartScene, typename... TArgs>
	void Init(SceneKey scene_key, TArgs&&... constructor_args) {
		static_assert(
			std::is_constructible_v<TStartScene, TArgs...>,
			"Start scene must be constructible from given arguments, check that start scene "
			"constructor is not private"
		);
		static_assert(
			std::is_convertible_v<TStartScene*, Scene*>, "Start scene must inherit from ptgn::Scene"
		);
		PTGN_ASSERT(!Has(impl::start_scene_key), "Cannot load more than one start scene");
		PTGN_ASSERT(scene_key == impl::start_scene_key);
		AddActive(impl::start_scene_key);
		Manager<std::shared_ptr<Scene>>::Load(
			scene_key, std::make_shared<TStartScene>(std::forward<TArgs>(constructor_args)...)
		);
		InitScene(scene_key);
	}

	void Reset();
	void Shutdown();

	void Update(float dt);
	void UnloadFlagged();
	/*void ExitAllExcept(std::size_t scene_key) {
		for (auto other_key : active_scenes_) {
			if (other_key != scene_key && Has(other_key)) {
				auto scene = Get(other_key);
				scene->Exit();
			}
		}
	}*/
	[[nodiscard]] bool ActiveScenesContain(std::size_t key) const;

private:
	std::int64_t flagged_{ 0 };
	std::vector<std::size_t> active_scenes_;
};

} // namespace ptgn
