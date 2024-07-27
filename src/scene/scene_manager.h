#pragma once

#include <vector>

#include "core/handle_manager.h"
#include "protegon/scene.h"
#include "utility/debug.h"
#include "utility/type_traits.h"

namespace ptgn {

using SceneKey = std::size_t;

namespace impl {

inline constexpr std::size_t start_scene_key{ 0 };

} // namespace impl

class SceneManager : public HandleManager<std::shared_ptr<Scene>> {
private:
	SceneManager()								 = default;
	~SceneManager()								 = default;
	SceneManager(const SceneManager&)			 = delete;
	SceneManager(SceneManager&&)				 = default;
	SceneManager& operator=(const SceneManager&) = delete;
	SceneManager& operator=(SceneManager&&)		 = default;

public:
	template <
		typename T, typename... TArgs, type_traits::constructible<T, TArgs...> = true,
		type_traits::convertible<T*, Scene*> = true>
	std::shared_ptr<T> Load(SceneKey scene_key, TArgs&&... constructor_args) {
		PTGN_CHECK(
			scene_key != impl::start_scene_key,
			"Cannot load scene with key == 0, it is reserved for the starting scene"
		);
		return std::static_pointer_cast<T>(HandleManager<std::shared_ptr<Scene>>::Load(
			scene_key, std::make_shared<T>(std::forward<TArgs>(constructor_args)...)
		));
	}

	template <
		typename TScene = Scene,
		type_traits::enable<(std::is_base_of_v<TScene, Scene> || std::is_same_v<TScene, Scene>)> =
			true>
	[[nodiscard]] std::shared_ptr<TScene> Get(SceneKey scene_key) {
		return std::static_pointer_cast<TScene>(HandleManager<std::shared_ptr<Scene>>::Get(scene_key
		));
	}

	virtual void Unload(std::size_t scene_key) override final;

	void SetActive(std::size_t scene_key);
	void AddActive(std::size_t scene_key);
	void RemoveActive(std::size_t scene_key);
	[[nodiscard]] std::vector<std::shared_ptr<Scene>> GetActive();

private:
	friend class Game;

	template <
		typename T, typename... TArgs, type_traits::constructible<T, TArgs...> = true,
		type_traits::convertible<T*, Scene*> = true>
	std::shared_ptr<T> LoadStartScene(SceneKey scene_key, TArgs&&... constructor_args) {
		PTGN_ASSERT(scene_key == impl::start_scene_key);
		return std::static_pointer_cast<T>(HandleManager<std::shared_ptr<Scene>>::Load(
			scene_key, std::make_shared<T>(std::forward<TArgs>(constructor_args)...)
		));
	}

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
