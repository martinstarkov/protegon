#pragma once

#include <concepts>
#include <memory>
#include <optional>
#include <vector>

#include "common/assert.h"
#include "common/concepts.h"
#include "scene/scene.h"
#include "scene/scene_key.h"
#include "scene/scene_transition.h"
#include "utility/file.h"
#include "utility/span.h"

namespace ptgn {

namespace impl {

class Game;

template <typename T>
concept SceneType = IsOrDerivedFrom<T, Scene>;

template <typename T>
concept SceneTransitionType = IsOrDerivedFrom<T, SceneTransition> || std::is_same_v<T, int>;

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
		bool first_scene{ scene_index_ == 0 };
		scene_index_++;
		auto [inserted, scene] = VectorTryEmplaceIf<TScene>(
			scenes_, [scene_key](const auto& s) { return s->GetKey() == scene_key; },
			std::forward<TArgs>(constructor_args)...
		);
		scene->first_scene_ = first_scene;
		scene->SetKey(scene_key);
		return *static_cast<TScene*>(scene.get());
	}

	template <SceneType TScene, typename... TArgs>
		requires std::constructible_from<TScene, TArgs...>
	TScene& Load(const SceneKey& scene_key, TArgs&&... constructor_args) {
		bool first_scene{ scene_index_ == 0 };
		scene_index_++;
		auto [replaced, scene] = VectorReplaceOrEmplaceIf<TScene>(
			scenes_, [scene_key](const auto& s) { return s->GetKey() == scene_key; },
			std::forward<TArgs>(constructor_args)...
		);
		scene->first_scene_ = first_scene;
		scene->SetKey(scene_key);
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

	// @param from_scene_key If nullopt, will transition from all currently active scenes.
	template <SceneType TScene, typename... TArgs>
		requires std::constructible_from<TScene, TArgs...>
	TScene& Transition(
		const std::optional<SceneKey>& from_scene_key, const SceneKey& to_scene_key,
		TArgs&&... constructor_args
	) {
		auto& scene{ Load<TScene>(to_scene_key, std::forward<TArgs>(constructor_args)...) };
		SceneManager::Transition(from_scene_key, to_scene_key);
		return scene;
	}

	// @param from_scene_key If nullopt, will transition from all currently active scenes.
	template <
		SceneType TScene, SceneTransitionType TransitionIn = int,
		SceneTransitionType TransitionOut = int, typename... TArgs>
		requires std::constructible_from<TScene, TArgs...>
	TScene& Transition(
		const std::optional<SceneKey>& from_scene_key, const SceneKey& to_scene_key,
		const TransitionIn& in, const TransitionOut& out, TArgs&&... constructor_args
	) {
		auto& scene{ Load<TScene>(to_scene_key, std::forward<TArgs>(constructor_args)...) };
		SceneManager::Transition<TransitionIn, TransitionOut>(
			from_scene_key, to_scene_key, in, out
		);
		return scene;
	}

	// @param from_scene_key If nullopt, will transition from all currently active scenes.
	void Transition(const std::optional<SceneKey>& from_scene_key, const SceneKey& to_scene_key) {
		SceneManager::Transition<int, int>(from_scene_key, to_scene_key, 0, 0);
	}

	// @param from_scene_key If nullopt, will transition from all currently active scenes.
	template <SceneTransitionType TransitionIn, SceneTransitionType TransitionOut = int>
	void Transition(
		const std::optional<SceneKey>& from_scene_key, const SceneKey& to_scene_key,
		TransitionIn in, TransitionOut out = 0
	) {
		if constexpr (!std::is_same_v<TransitionOut, int>) {
			const auto transition_out_func = [](auto& scene_from) {
				auto transition_out		= std::make_shared<TransitionOut>(out);
				transition_out->scene	= scene_from.get();
				scene_from->transition_ = transition_out;
			};

			if (from_scene_key.has_value()) {
				auto scene_from{ GetImpl(*from_scene_key) };
				transition_out_func(scene_from);
			} else {
				for (const auto& scene_from : active_scenes_) {
					transition_out_func(scene_from);
				}
			}
		}
		if constexpr (!std::is_same_v<TransitionIn, int>) {
			auto scene_to{ GetImpl(to_scene_key) };
			auto transition_in	  = std::make_shared<TransitionIn>(in);
			transition_in->scene  = scene_to.get();
			scene_to->transition_ = transition_in;
		}
		if (from_scene_key.has_value()) {
			Exit(*from_scene_key);
		} else {
			ExitAll();
		}
		Enter(to_scene_key);
	}

	void Enter(const SceneKey& scene_key);

	void EnterConfig(const path& scene_json_file);

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

	// Move a scene up in the scene list.
	void MoveUp(const SceneKey& scene_key);

	// Move a scene down in the scene list.
	void MoveDown(const SceneKey& scene_key);

	// Bring a scene to the top of the scene list.
	void BringToTop(const SceneKey& scene_key);

	// Move a scene to the bottom of the scene list.
	void MoveToBottom(const SceneKey& scene_key);

	// Move a scene above another scene in the scene list.
	void MoveAbove(const SceneKey& source_key, const SceneKey& target_key);

	// Move a scene below another scene in the scene list.
	void MoveBelow(const SceneKey& source_key, const SceneKey& target_key);

private:
	friend class Game;

	void ExitAll();

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
	std::shared_ptr<Scene> current_;

	std::size_t scene_index_{ 0 };
};

} // namespace impl

} // namespace ptgn