#pragma once

#include <cstdint>
#include <memory>
#include <type_traits>
#include <vector>

#include "core/manager.h"
#include "protegon/scene.h"
#include "utility/debug.h"
#include "utility/type_traits.h"

namespace ptgn::impl {

inline constexpr std::size_t start_scene_key{ 0 };

class SceneManager : public Manager<std::shared_ptr<Scene>> {
public:
	using Manager::Manager;

	template <
		typename T, typename TKey, typename... TArgs, tt::constructible<T, TArgs...> = true,
		tt::convertible<T*, Scene*> = true>
	std::shared_ptr<T> Load(const TKey& scene_key, TArgs&&... constructor_args) {
		auto k{ GetInternalKey(scene_key) };
		PTGN_ASSERT(
			k != impl::start_scene_key,
			"Cannot load scene with a key for which Hash(key) returns 0, it is reserved for the "
			"starting scene"
		);
		auto scene{ std::make_shared<T>(std::forward<TArgs>(constructor_args)...) };
		PTGN_ASSERT(!scene->actions_.empty());
		return std::static_pointer_cast<T>(
			Manager<std::shared_ptr<Scene>>::Load(k, std::move(scene))
		);
	}

	template <typename TScene = Scene, typename TKey = Key>
	[[nodiscard]] std::shared_ptr<TScene> Get(const TKey& scene_key) {
		static_assert(
			std::is_base_of_v<Scene, TScene> || std::is_same_v<TScene, Scene>,
			"Cannot cast retrieved scene to type which does not inherit from the Scene class"
		);
		return std::static_pointer_cast<TScene>(Manager<std::shared_ptr<Scene>>::Get(scene_key));
	}

	template <typename TKey>
	void Unload(const TKey& scene_key) {
		UnloadImpl(GetInternalKey(scene_key));
	}

	template <typename TKey>
	void AddActive(const TKey& scene_key) {
		AddActiveImpl(GetInternalKey(scene_key));
	}

	template <typename TKey>
	void RemoveActive(const TKey& scene_key) {
		RemoveActiveImpl(GetInternalKey(scene_key));
	}

	void ClearActive();
	void UnloadAll();

	[[nodiscard]] std::vector<std::shared_ptr<Scene>> GetActive();

	[[nodiscard]] Scene& GetTopActive();

private:
	void InitScene(const InternalKey& scene_key);

	void UnloadImpl(const InternalKey& scene_key);

	void AddActiveImpl(const InternalKey& scene_key);
	void RemoveActiveImpl(const InternalKey& scene_key);

	friend class Game;

	template <typename TStartScene, typename... TArgs>
	void Init(const InternalKey& scene_key, TArgs&&... constructor_args) {
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
		auto& start_scene = Manager<std::shared_ptr<Scene>>::Load(
			scene_key, std::make_shared<TStartScene>(std::forward<TArgs>(constructor_args)...)
		);
		AddActive(impl::start_scene_key);
		InitScene(scene_key);
	}

	void Reset();
	void Shutdown();

	void Update();

	// @return True if scene was changed, false otherwise.
	bool UpdateFlagged();

	[[nodiscard]] bool ActiveScenesContain(const InternalKey& scene_key) const;

private:
	std::vector<InternalKey> active_scenes_;
};

} // namespace ptgn::impl
