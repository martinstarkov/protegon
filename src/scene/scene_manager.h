#pragma once

#include <concepts>
#include <memory>
#include <vector>

#include "common/assert.h"
#include "common/concepts.h"
#include "scene/scene.h"
#include "scene/scene_key.h"
#include "scene/scene_transition.h"
#include "utility/span.h"

namespace ptgn {

namespace impl {

class Game;

template <typename T>
concept SceneType = IsOrDerivedFrom<T, Scene>;

template <typename T>
concept SceneTransitionType = IsOrDerivedFrom<T, SceneTransition> || std::is_same_v<T, void>;

class SceneManager {
public:
	SceneManager()									 = default;
	~SceneManager()									 = default;
	SceneManager(SceneManager&&) noexcept			 = default;
	SceneManager& operator=(SceneManager&&) noexcept = default;
	SceneManager(const SceneManager&)				 = delete;
	SceneManager& operator=(const SceneManager&)	 = delete;

	template <SceneType TScene, typename... TArgs>
		requires std::constructible_from<TScene, TArgs...>
	TScene& TryLoad(const SceneKey& scene_key, TArgs&&... constructor_args) {
		auto [inserted, scene] = VectorTryEmplaceIf<TScene>(
			queued_scenes_, [scene_key](const auto& s) { return s->key_ == scene_key; },
			std::forward<TArgs>(constructor_args)...
		);
		if (inserted) {
			scene->SetKey(scene_key);
		}
		return *static_cast<TScene*>(scene.get());
	}

	template <SceneType TScene, typename... TArgs>
		requires std::constructible_from<TScene, TArgs...>
	TScene& Load(const SceneKey& scene_key, TArgs&&... constructor_args) {
		auto [replaced, scene] = VectorReplaceOrEmplaceIf<TScene>(
			queued_scenes_, [scene_key](const auto& s) { return s->key_ == scene_key; },
			std::forward<TArgs>(constructor_args)...
		);
		if (!replaced) {
			scene->SetKey(scene_key);
		}
		return *static_cast<TScene*>(scene.get());
	}

	template <SceneType TScene, typename... TArgs>
		requires std::constructible_from<TScene, TArgs...>
	TScene& Enter(const SceneKey& scene_key, TArgs&&... constructor_args) {
		auto& scene{ Load<TScene>(scene_key, std::forward<TArgs>(constructor_args)...) };
		[[maybe_unused]] auto keep_alive{ GetImpl(scene_key) };
		Enter(scene_key);
		return scene;
	}

	template <SceneType TScene, SceneTransitionType Transition = void, typename... TArgs>
		requires std::constructible_from<TScene, TArgs...>
	TScene& Transition(
		const SceneKey& from_scene_key, const SceneKey& to_scene_key, TArgs&&... constructor_args
	) {
		auto& scene{ Load<TScene>(to_scene_key, std::forward<TArgs>(constructor_args)...) };
		SceneManager::Transition<Transition>(from_scene_key, to_scene_key);
		return scene;
	}

	void Transition(const SceneKey& from_scene_key, const SceneKey& to_scene_key) {
		SceneManager::Transition<void>(from_scene_key, to_scene_key);
	}

	template <SceneTransitionType Transition>
	void Transition(const SceneKey& from_scene_key, const SceneKey& to_scene_key) {
		if constexpr (std::is_same_v<Transition, void>) {
			Exit(from_scene_key);
			Enter(to_scene_key);
		}
		// TODO: Fix.

		/*if (transition == SceneTransition{}) {
			ExitImpl(from_scene_key);
			EnterImpl(to_scene_key);
			return;
		}

		if (HasActiveScene(to_scene_key)) {
			return;
		}
		auto from{ GetScene(from_scene_key).Get<SceneComponent>().scene.get() };
		auto to{ GetScene(to_scene_key).Get<SceneComponent>().scene.get() };
		transition.Start(false, from_scene_key, to_scene_key, from);
		transition.Start(true, to_scene_key, from_scene_key, to);*/
	}

	void Enter(const SceneKey& scene_key);

	void Unload(const SceneKey& scene_key);

	void Exit(const SceneKey& scene_key);

	template <SceneType TScene = Scene>
	[[nodiscard]] TScene& Get(const SceneKey& scene_key) {
		auto scene{ GetImpl(scene_key) };
		PTGN_ASSERT(scene, "Cannot retrieve scene which does not exist in the scene manager");
		return *static_cast<TScene*>(scene.get());
	}

	[[nodiscard]] const Scene& GetCurrent() const;

	[[nodiscard]] Scene& GetCurrent();

	[[nodiscard]] bool Has(const SceneKey& scene_key) const;

	[[nodiscard]] bool IsActive(const SceneKey& scene_key) const;

private:
	friend class Game;

	std::shared_ptr<Scene> GetImpl(const SceneKey& scene_key) const;

	// Updates all the active scenes.
	void Update(Game& game);

	// Clears the frame buffers of each scene.
	// void ClearSceneTargets();

	void Reset();
	void Shutdown();

	void HandleSceneEvents();

	std::vector<std::shared_ptr<Scene>> active_scenes_;
	std::vector<std::shared_ptr<Scene>> scenes_;
	std::vector<std::shared_ptr<Scene>> queued_scenes_;
	std::shared_ptr<Scene> current_;
};

} // namespace impl

} // namespace ptgn