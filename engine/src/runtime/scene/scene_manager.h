#pragma once

#include <concepts>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "core/assert.h"
#include "core/util/hash.h"
#include "core/util/time.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_common.h"
#include "runtime/scene/scene_registry.h"
#include "runtime/scene/scene_transition.h"

namespace ptgn {

class Application;

struct SceneTransitionPriority {
	explicit SceneTransitionPriority() = default;

	/// @brief Explicit construction prevents conflict with scene constructor args.
	explicit SceneTransitionPriority(std::size_t value) : value{ value } {}

	std::size_t value{ 0 };
};

namespace impl {

class ApplicationContext;

class SceneManager {
public:
	enum class CommandType {
		Enter,
		Exit,
		ReEnter
	};

	struct Command {
		CommandType type{ CommandType::Enter };

		std::string to_scene_tag;
		std::size_t to_scene_tag_hash{ 0 };

		SceneTransitionPriority priority;

		SceneFactory scene_factory;

		std::unique_ptr<SceneTransition> transition_out;
		std::unique_ptr<SceneTransition> transition_in;
	};

	template <typename... TArgs>
		requires std::constructible_from<Command, TArgs...>
	void PushCommand(TArgs&&... args) {
		commands_.emplace_back(std::forward<TArgs>(args)...);
	}

	[[nodiscard]] bool HasScene(std::size_t scene_tag_hash) const;

	const std::vector<std::unique_ptr<Scene>>& GetScenes() const;
	std::vector<std::unique_ptr<Scene>>& GetScenes();

	const Scene& GetScene(std::size_t scene_tag_hash) const;
	Scene& GetScene(std::size_t scene_tag_hash);

	template <SceneType T, SceneTransitionType TransitionIn, typename... TArgs>
		requires std::constructible_from<T, TArgs...>
	bool Enter(
		std::string_view scene_tag, TransitionIn&& transition_in, SceneTransitionPriority priority,
		TArgs&&... constructor_args
	) {
		auto scene_tag_hash{ Hash(scene_tag) };
		if (!CanIssueCommands(scene_tag_hash)) {
			return false;
		}

		if (HasScene(scene_tag_hash)) {
			return ReEnter<T>(
				scene_tag, SceneTransitionPair{ {}, std::forward<TransitionIn>(transition_in) },
				std::forward<TArgs>(constructor_args)...
			);
		}

		std::unique_ptr<SceneTransition> transition_in_ptr;

		if constexpr (!std::same_as<std::decay_t<TransitionIn>, NoTransition>) {
			transition_in_ptr = std::make_unique<std::decay_t<TransitionIn>>(
				std::forward<TransitionIn>(transition_in)
			);
		}

		PushCommand(
			impl::SceneManager::CommandType::Enter, std::string{ scene_tag }, scene_tag_hash,
			priority, GetFactory<T>(std::forward<TArgs>(constructor_args)...), nullptr,
			std::move(transition_in_ptr)
		);

		return true;
	}

	template <SceneType T, SceneTransitionType TransitionIn, typename... TArgs>
		requires std::constructible_from<T, TArgs...>
	bool Enter(
		std::string_view scene_tag, TransitionIn&& transition_in, TArgs&&... constructor_args
	) {
		return Enter<T>(
			scene_tag, std::forward<TransitionIn>(transition_in), SceneTransitionPriority{},
			std::forward<TArgs>(constructor_args)...
		);
	}

	template <SceneType T, typename... TArgs>
		requires std::constructible_from<T, TArgs...>
	bool Enter(
		std::string_view scene_tag, SceneTransitionPriority priority, TArgs&&... constructor_args
	) {
		return Enter<T>(
			scene_tag, NoTransition{}, priority, std::forward<TArgs>(constructor_args)...
		);
	}

	template <SceneType T, typename... TArgs>
		requires std::constructible_from<T, TArgs...>
	bool Enter(std::string_view scene_tag, TArgs&&... constructor_args) {
		return Enter<T>(
			scene_tag, NoTransition{}, SceneTransitionPriority{},
			std::forward<TArgs>(constructor_args)...
		);
	}

	template <SceneTransitionType TransitionOut>
	bool Exit(
		std::string_view scene_tag, TransitionOut&& transition_out,
		SceneTransitionPriority priority = SceneTransitionPriority{ 0 }
	) {
		auto scene_tag_hash{ Hash(scene_tag) };
		if (!CanIssueCommands(scene_tag_hash)) {
			return false;
		}

		if (!HasScene(scene_tag_hash)) {
			return false;
		}

		std::unique_ptr<SceneTransition> transition_out_ptr;

		if constexpr (!std::same_as<std::decay_t<TransitionOut>, NoTransition>) {
			transition_out_ptr = std::make_unique<std::decay_t<TransitionOut>>(
				std::forward<TransitionOut>(transition_out)
			);
		}

		PushCommand(
			impl::SceneManager::CommandType::Exit, std::string{ scene_tag }, scene_tag_hash,
			priority, nullptr, std::move(transition_out_ptr), nullptr
		);

		return true;
	}

	bool Exit(
		std::string_view scene_tag, SceneTransitionPriority priority = SceneTransitionPriority{ 0 }
	) {
		return Exit(scene_tag, NoTransition{}, priority);
	}

	template <
		SceneType T, SceneTransitionType TransitionOut, SceneTransitionType TransitionIn,
		typename... TArgs>
		requires std::constructible_from<T, TArgs...>
	bool ReEnter(
		std::string_view scene_tag, SceneTransitionPair<TransitionOut, TransitionIn>&& transition,
		TArgs&&... constructor_args
	) {
		auto scene_tag_hash{ Hash(scene_tag) };

		if (!CanIssueCommands(scene_tag_hash)) {
			return false;
		}

		PTGN_ASSERT(
			HasScene(scene_tag_hash), "Cannot re-enter a scene tag hash which has not been entered"
		);

		std::unique_ptr<SceneTransition> transition_out_ptr;
		std::unique_ptr<SceneTransition> transition_in_ptr;

		if constexpr (!std::same_as<std::decay_t<TransitionOut>, NoTransition>) {
			transition_out_ptr =
				std::make_unique<std::decay_t<TransitionOut>>(std::move(transition.out));
		}

		if constexpr (!std::same_as<std::decay_t<TransitionIn>, NoTransition>) {
			transition_in_ptr =
				std::make_unique<std::decay_t<TransitionIn>>(std::move(transition.in));
		}

		PushCommand(
			impl::SceneManager::CommandType::ReEnter, std::string{ scene_tag }, scene_tag_hash,
			SceneTransitionPriority{ std::numeric_limits<std::size_t>::max() },
			GetFactory<T>(std::forward<TArgs>(constructor_args)...), std::move(transition_out_ptr),
			std::move(transition_in_ptr)
		);

		return true;
	}

	template <SceneType T, typename... TArgs>
		requires std::constructible_from<T, TArgs...>
	bool ReEnter(std::string_view scene_tag, TArgs&&... constructor_args) {
		return ReEnter<T>(
			scene_tag, SceneTransitionPair{}, std::forward<TArgs>(constructor_args)...
		);
	}

	template <
		SceneType ToScene, SceneTransitionType TransitionOut, SceneTransitionType TransitionIn,
		typename... TArgs>
		requires std::constructible_from<ToScene, TArgs...>
	bool Transition(
		std::string_view from_scene_tag, std::string_view to_scene_tag,
		SceneTransitionPair<TransitionOut, TransitionIn>&& transition,
		SceneTransitionPriority priority, TArgs&&... to_scene_constructor_args
	) {
		if (from_scene_tag == to_scene_tag) {
			return ReEnter<ToScene>(
				to_scene_tag, std::move(transition),
				std::forward<TArgs>(to_scene_constructor_args)...
			);
		}

		bool exited{ Exit(from_scene_tag, std::move(transition.out), priority) };
		bool entered{ Enter<ToScene>(
			to_scene_tag, std::move(transition.in), priority,
			std::forward<TArgs>(to_scene_constructor_args)...
		) };
		return exited || entered;
	}

	template <
		SceneType ToScene, SceneTransitionType TransitionOut, SceneTransitionType TransitionIn,
		typename... TArgs>
		requires std::constructible_from<ToScene, TArgs...>
	bool Transition(
		std::string_view from_scene_tag, std::string_view to_scene_tag,
		SceneTransitionPair<TransitionOut, TransitionIn>&& transition,
		TArgs&&... to_scene_constructor_args
	) {
		return Transition<ToScene>(
			from_scene_tag, to_scene_tag, std::move(transition), SceneTransitionPriority{},
			std::forward<TArgs>(to_scene_constructor_args)...
		);
	}

	template <SceneType ToScene, typename... TArgs>
		requires std::constructible_from<ToScene, TArgs...>
	bool Transition(
		std::string_view from_scene_tag, std::string_view to_scene_tag,
		SceneTransitionPriority priority, TArgs&&... to_scene_constructor_args
	) {
		return Transition<ToScene>(
			from_scene_tag, to_scene_tag, SceneTransitionPair{}, priority,
			std::forward<TArgs>(to_scene_constructor_args)...
		);
	}

	template <SceneType ToScene, typename... TArgs>
		requires std::constructible_from<ToScene, TArgs...>
	bool Transition(
		std::string_view from_scene_tag, std::string_view to_scene_tag,
		TArgs&&... to_scene_constructor_args
	) {
		return Transition<ToScene>(
			from_scene_tag, to_scene_tag, SceneTransitionPair{}, SceneTransitionPriority{},
			std::forward<TArgs>(to_scene_constructor_args)...
		);
	}

private:
	friend class ptgn::Application;
	friend class ApplicationContext;

	SceneManager()									 = default;
	~SceneManager() noexcept						 = default;
	SceneManager(SceneManager&&) noexcept			 = delete;
	SceneManager& operator=(SceneManager&&) noexcept = delete;
	SceneManager(const SceneManager&)				 = delete;
	SceneManager& operator=(const SceneManager&)	 = delete;

	template <SceneType T, typename... TArgs>
	[[nodiscard]] static SceneFactory GetFactory(TArgs&&... constructor_args) {
		return [constructor_args...](
				   Application& app, SceneData&& scene_data
			   ) -> std::unique_ptr<Scene> {
			auto scene{ std::make_unique<T>(constructor_args...) };
			scene->Init(app, std::move(scene_data));
			return scene;
		};
	}

	void PreUpdate();
	void OnEvent();
	void Update(Application& app, secondsf dt);
	void Draw() const;

	struct ReEnteringScene {
		std::size_t scene_tag_hash{ 0 };
		std::size_t temporary_scene_tag_hash{ 0 };
	};

	/// @return A map of scene tag hashes to the highest priority command for each scene (if a
	/// command was issued).
	std::unordered_map<std::size_t, Command> GetTopPriorityCommands();
	void ApplyCommands(
		Application& app, std::unordered_map<std::size_t, Command>& top_priority_commands
	);
	void UpdateTransitions(secondsf dt);
	void UpdateReEnteredSceneTagHashes();

	[[nodiscard]] std::size_t GenerateTempTagHash() const;

	[[nodiscard]] bool CanIssueCommands(std::size_t target_scene_tag_hash) const;

	std::vector<std::unique_ptr<Scene>> scenes_;

	std::vector<Command> commands_;

	/// @brief Contains the scene tag hashes of currently re-entering scenes.
	std::vector<ReEnteringScene> reentering_scenes_;
};

} // namespace impl

class LocalSceneManager {
public:
	template <SceneType T, SceneTransitionType TransitionIn, typename... TArgs>
		requires std::constructible_from<T, TArgs...>
	bool Enter(
		std::string_view scene_tag, TransitionIn&& transition_in, SceneTransitionPriority priority,
		TArgs&&... constructor_args
	) {
		return scene_manager_.Enter<T>(
			scene_tag, std::forward<TransitionIn>(transition_in), priority,
			std::forward<TArgs>(constructor_args)...
		);
	}

	template <SceneType T, SceneTransitionType TransitionIn, typename... TArgs>
		requires std::constructible_from<T, TArgs...>
	bool Enter(
		std::string_view scene_tag, TransitionIn&& transition_in, TArgs&&... constructor_args
	) {
		return Enter<T>(
			scene_tag, std::forward<TransitionIn>(transition_in), SceneTransitionPriority{},
			std::forward<TArgs>(constructor_args)...
		);
	}

	template <SceneType T, typename... TArgs>
		requires std::constructible_from<T, TArgs...>
	bool Enter(
		std::string_view scene_tag, SceneTransitionPriority priority, TArgs&&... constructor_args
	) {
		return Enter<T>(
			scene_tag, NoTransition{}, priority, std::forward<TArgs>(constructor_args)...
		);
	}

	template <SceneType T, typename... TArgs>
		requires std::constructible_from<T, TArgs...>
	bool Enter(std::string_view scene_tag, TArgs&&... constructor_args) {
		return Enter<T>(
			scene_tag, NoTransition{}, SceneTransitionPriority{},
			std::forward<TArgs>(constructor_args)...
		);
	}

	template <SceneTransitionType TransitionOut>
	bool Exit(
		std::string_view scene_tag, TransitionOut&& transition_out,
		SceneTransitionPriority priority = SceneTransitionPriority{ 0 }
	) {
		return scene_manager_.Exit(
			scene_tag, std::forward<TransitionOut>(transition_out), priority
		);
	}

	bool Exit(
		std::string_view scene_tag, SceneTransitionPriority priority = SceneTransitionPriority{ 0 }
	) {
		return Exit(scene_tag, NoTransition{}, priority);
	}

	template <
		SceneType T, SceneTransitionType TransitionOut, SceneTransitionType TransitionIn,
		typename... TArgs>
		requires std::constructible_from<T, TArgs...>
	bool ReEnter(
		std::string_view scene_tag, SceneTransitionPair<TransitionOut, TransitionIn>&& transition,
		TArgs&&... constructor_args
	) {
		return scene_manager_.ReEnter<T>(
			scene_tag, std::move(transition), std::forward<TArgs>(constructor_args)...
		);
	}

	template <SceneType T, typename... TArgs>
		requires std::constructible_from<T, TArgs...>
	bool ReEnter(std::string_view scene_tag, TArgs&&... constructor_args) {
		return ReEnter<T>(
			scene_tag, SceneTransitionPair{}, std::forward<TArgs>(constructor_args)...
		);
	}

	template <
		SceneType ToScene, SceneTransitionType TransitionOut, SceneTransitionType TransitionIn,
		typename... TArgs>
		requires std::constructible_from<ToScene, TArgs...>
	bool Transition(
		std::string_view from_scene_tag, std::string_view to_scene_tag,
		SceneTransitionPair<TransitionOut, TransitionIn>&& transition,
		SceneTransitionPriority priority, TArgs&&... to_scene_constructor_args
	) {
		return scene_manager_.Transition<ToScene>(
			from_scene_tag, to_scene_tag, std::move(transition), priority,
			std::forward<TArgs>(to_scene_constructor_args)...
		);
	}

	template <
		SceneType ToScene, SceneTransitionType TransitionOut, SceneTransitionType TransitionIn,
		typename... TArgs>
		requires std::constructible_from<ToScene, TArgs...>
	bool Transition(
		std::string_view from_scene_tag, std::string_view to_scene_tag,
		SceneTransitionPair<TransitionOut, TransitionIn>&& transition,
		TArgs&&... to_scene_constructor_args
	) {
		return Transition<ToScene>(
			from_scene_tag, to_scene_tag, std::move(transition), SceneTransitionPriority{},
			std::forward<TArgs>(to_scene_constructor_args)...
		);
	}

	template <SceneType ToScene, typename... TArgs>
		requires std::constructible_from<ToScene, TArgs...>
	bool Transition(
		std::string_view from_scene_tag, std::string_view to_scene_tag,
		SceneTransitionPriority priority, TArgs&&... to_scene_constructor_args
	) {
		return Transition<ToScene>(
			from_scene_tag, to_scene_tag, SceneTransitionPair{}, priority,
			std::forward<TArgs>(to_scene_constructor_args)...
		);
	}

	template <SceneType ToScene, typename... TArgs>
		requires std::constructible_from<ToScene, TArgs...>
	bool Transition(
		std::string_view from_scene_tag, std::string_view to_scene_tag,
		TArgs&&... to_scene_constructor_args
	) {
		return Transition<ToScene>(
			from_scene_tag, to_scene_tag, SceneTransitionPair{}, SceneTransitionPriority{},
			std::forward<TArgs>(to_scene_constructor_args)...
		);
	}

	template <
		SceneType ToScene, SceneTransitionType TransitionOut, SceneTransitionType TransitionIn,
		typename... TArgs>
		requires std::constructible_from<ToScene, TArgs...>
	bool Switch(
		std::string_view scene_tag, SceneTransitionPair<TransitionOut, TransitionIn>&& transition,
		SceneTransitionPriority priority, TArgs&&... constructor_args
	) {
		if (!CanIssueCommands()) {
			return false;
		}

		return Transition<ToScene>(
			scene_.GetTag(), scene_tag, std::move(transition), priority,
			std::forward<TArgs>(constructor_args)...
		);
	}

	template <
		SceneType ToScene, SceneTransitionType TransitionOut, SceneTransitionType TransitionIn,
		typename... TArgs>
		requires std::constructible_from<ToScene, TArgs...>
	bool Switch(
		std::string_view scene_tag, SceneTransitionPair<TransitionOut, TransitionIn>&& transition,
		TArgs&&... constructor_args
	) {
		return Switch<ToScene>(
			scene_tag, std::move(transition), SceneTransitionPriority{},
			std::forward<TArgs>(constructor_args)...
		);
	}

	template <SceneType ToScene, typename... TArgs>
		requires std::constructible_from<ToScene, TArgs...>
	bool Switch(
		std::string_view scene_tag, SceneTransitionPriority priority, TArgs&&... constructor_args
	) {
		return Switch<ToScene>(
			scene_tag, SceneTransitionPair{}, priority, std::forward<TArgs>(constructor_args)...
		);
	}

	template <SceneType ToScene, typename... TArgs>
		requires std::constructible_from<ToScene, TArgs...>
	bool Switch(std::string_view scene_tag, TArgs&&... constructor_args) {
		return Switch<ToScene>(
			scene_tag, SceneTransitionPair{}, SceneTransitionPriority{},
			std::forward<TArgs>(constructor_args)...
		);
	}

private:
	friend class Scene;
	friend class SceneContext;

	explicit LocalSceneManager(impl::SceneManager& scene_manager, Scene& scene);

	[[nodiscard]] bool CanIssueCommands() const;

	impl::SceneManager& scene_manager_;
	Scene& scene_;
};

} // namespace ptgn