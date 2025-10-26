#pragma once

#include <concepts>
#include <memory>
#include <optional>
#include <vector>

#include "core/util/concepts.h"
#include "core/util/file.h"
#include "core/util/span.h"
#include "debug/runtime/assert.h"
#include "world/scene/scene.h"
#include "world/scene/scene_key.h"
#include "world/scene/scene_transition.h"

namespace ptgn {

class Application;

namespace impl {

template <typename T>
concept SceneType = IsOrDerivedFrom<T, Scene>;

} // namespace impl

class SceneManager {
public:
	SceneManager()									 = default;
	~SceneManager()									 = default;
	SceneManager(SceneManager&&) noexcept			 = delete;
	SceneManager& operator=(SceneManager&&) noexcept = delete;
	SceneManager(const SceneManager&)				 = delete;
	SceneManager& operator=(const SceneManager&)	 = delete;

	template <impl::SceneType TScene, typename... TArgs>
		requires std::constructible_from<TScene, TArgs...>
	TScene& TryLoad(const SceneKey& scene_key, TArgs&&... constructor_args) {
		scene_index_++;
		auto [inserted, scene] = VectorTryEmplaceIf<TScene>(
			scenes_, [scene_key](const auto& s) { return s->GetKey() == scene_key; },
			std::forward<TArgs>(constructor_args)...
		);
		scene->SetKey(scene_key);
		return *static_cast<TScene*>(scene.get());
	}

	template <impl::SceneType TScene, typename... TArgs>
		requires std::constructible_from<TScene, TArgs...>
	TScene& Load(const SceneKey& scene_key, TArgs&&... constructor_args) {
		scene_index_++;
		auto [replaced, scene] = VectorReplaceOrEmplaceIf<TScene>(
			scenes_, [scene_key](const auto& s) { return s->GetKey() == scene_key; },
			std::forward<TArgs>(constructor_args)...
		);
		scene->SetKey(scene_key);
		return *static_cast<TScene*>(scene.get());
	}

	template <impl::SceneType TScene, typename... TArgs>
		requires std::constructible_from<TScene, TArgs...>
	TScene& Enter(const SceneKey& scene_key, TArgs&&... constructor_args) {
		auto& scene{ Load<TScene>(scene_key, std::forward<TArgs>(constructor_args)...) };
		[[maybe_unused]] auto keep_alive{ GetImpl(scene_key) };
		Enter(scene_key);
		return scene;
	}

	// @param from_scene_key If nullopt, will transition from all currently active scenes.
	template <
		impl::SceneType TScene, impl::SceneTransitionType TransitionIn = NoTransition,
		impl::SceneTransitionType TransitionOut = NoTransition, typename... TArgs>
		requires std::constructible_from<TScene, TArgs...>
	TScene& Transition(
		const std::optional<SceneKey>& from_scene_key, const SceneKey& to_scene_key,
		TransitionIn&& in = {}, TransitionOut&& out = {}, TArgs&&... args
	) {
		auto& scene{ Load<TScene>(to_scene_key, std::forward<TArgs>(args)...) };

		if constexpr (!std::same_as<std::remove_cvref_t<TransitionOut>, NoTransition>) {
			auto set_out = [&](auto& scene_from) {
				auto t = std::make_shared<std::remove_cvref_t<TransitionOut>>(
					std::forward<TransitionOut>(out)
				);
				t->scene				= scene_from.get();
				scene_from->transition_ = std::move(t);
			};
			if (from_scene_key) {
				set_out(GetImpl(*from_scene_key));
			} else {
				for (auto& s : active_scenes_) {
					set_out(s);
				}
			}
		}

		if constexpr (!std::same_as<std::remove_cvref_t<TransitionIn>, NoTransition>) {
			auto scene_to = GetImpl(to_scene_key);
			auto t =
				std::make_shared<std::remove_cvref_t<TransitionIn>>(std::forward<TransitionIn>(in));
			t->scene			  = scene_to.get();
			scene_to->transition_ = std::move(t);
		}

		if (from_scene_key) {
			Exit(*from_scene_key);
		} else {
			ExitAll();
		}
		Enter(to_scene_key);
		return scene;
	}

	void Enter(const SceneKey& scene_key);

	void EnterConfig(const path& scene_json_file);

	void Unload(const SceneKey& scene_key);

	void Exit(const SceneKey& scene_key);

	[[nodiscard]] std::shared_ptr<Scene> Get(const SceneKey& key) {
		auto p = GetImpl(key);
		PTGN_ASSERT(p, "Cannot retrieve scene which does not exist in the scene manager");
		return p;
	}

	[[nodiscard]] std::shared_ptr<const Scene> Get(const SceneKey& key) const {
		auto p = GetImpl(key);
		PTGN_ASSERT(p, "Cannot retrieve scene which does not exist in the scene manager");
		return std::static_pointer_cast<const Scene>(p);
	}

	template <impl::SceneType TScene /*= Scene*/>
	[[nodiscard]] std::shared_ptr<TScene> Get(const SceneKey& key) {
		auto base = GetImpl(key);
		PTGN_ASSERT(base, "Cannot retrieve scene which does not exist in the scene manager");

		if constexpr (std::is_same_v<std::remove_cv_t<TScene>, Scene>) {
			return std::static_pointer_cast<Scene>(base);
		} else {
			auto d = std::dynamic_pointer_cast<TScene>(base);
			PTGN_ASSERT(d, "Requested scene type does not match stored type for key");
			return d;
		}
	}

	template <impl::SceneType TScene /*= Scene*/>
	[[nodiscard]] std::shared_ptr<const TScene> Get(const SceneKey& key) const {
		auto base = GetImpl(key);
		PTGN_ASSERT(base, "Cannot retrieve scene which does not exist in the scene manager");

		if constexpr (std::is_same_v<std::remove_cv_t<TScene>, Scene>) {
			return std::static_pointer_cast<const Scene>(base);
		} else {
			auto d = std::dynamic_pointer_cast<const TScene>(base);
			PTGN_ASSERT(d, "Requested scene type does not match stored type for key");
			return d;
		}
	}

	[[nodiscard]] std::shared_ptr<const Scene> GetCurrent() const;
	[[nodiscard]] std::shared_ptr<Scene> GetCurrent();

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
	friend class Application;

	void ExitAll();

	std::shared_ptr<Scene> GetImpl(const SceneKey& scene_key) const;

	// Updates all the active scenes.
	void Update();

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

} // namespace ptgn