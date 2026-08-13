#pragma once

#include <concepts>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/util/hash.h"
#include "core/util/time.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_common.h"
#include "runtime/scene/scene_registry.h"
#include "runtime/scene/scene_transition.h"

namespace ptgn {

class Application;
class DrawContext;

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

	/// @brief Renames a loaded scene key without reconstructing the scene.
	bool RenameScene(
		std::string_view current_key,
		std::string_view new_key
	);

	/// @brief Moves one loaded scene within the SceneManager draw/update order.
	bool MoveScene(
		std::size_t from_index,
		std::size_t to_index
	);

	/// @brief Reorders the runtime or non runtime subset to match the supplied keys.
	void ReorderScenes(
		std::span<const std::string> ordered_keys,
		bool runtime
	);

	const Scene& GetScene(std::size_t scene_tag_hash) const;
	Scene& GetScene(std::size_t scene_tag_hash);

	/// @brief Enters a scene through an already constructed type-erased factory.
	/// Used by project-file loading and editor play mode.
	bool EnterFactory(
		std::string_view scene_tag, SceneFactory scene_factory,
		SceneTransitionPriority priority = SceneTransitionPriority{}
	);

	/// @brief Replaces an active scene through a type-erased factory without a transition.
	bool ReEnterFactory(std::string_view scene_tag, SceneFactory scene_factory);

	/// @brief Creates a type-erased factory for a scene registered with PTGN_REGISTER_SCENE.
	[[nodiscard]] static SceneFactory MakeRegisteredFactory(
		std::string scene_type, json parameters = json::object()
	) {
		auto type{ std::make_shared<const std::string>(std::move(scene_type)) };
		auto values{ std::make_shared<const json>(std::move(parameters)) };

		SceneFactory::Construct construct = [type, values](
			Application& app, SceneData&& scene_data
		) -> std::unique_ptr<Scene> {
			const auto& registration{ GetSceneRegistration(*type) };
			auto scene{ registration.construct(*values) };
			scene->Init(app, std::move(scene_data));
			return scene;
		};

		SceneFactory::Preload preload = [type, values](Application&) {
			return GetSceneRegistration(*type).preload_dependencies(*values);
		};

		return SceneFactory{ std::move(construct), std::move(preload) };
	}

	/// @brief Enters a scene through a type-erased factory with an optional transition.
	bool EnterFactory(
		std::string_view scene_tag, SceneFactory scene_factory,
		std::unique_ptr<SceneTransition> transition_in,
		SceneTransitionPriority priority = SceneTransitionPriority{}
	) {
		const auto scene_tag_hash{ Hash(scene_tag) };
		if (!CanIssueCommands(scene_tag_hash)) {
			return false;
		}

		if (HasScene(scene_tag_hash)) {
			return ReEnterFactory(
				scene_tag, std::move(scene_factory), nullptr, std::move(transition_in)
			);
		}

		PushCommand(
			CommandType::Enter, std::string{ scene_tag }, scene_tag_hash, priority,
			std::move(scene_factory), nullptr, std::move(transition_in)
		);
		return true;
	}

	/// @brief Replaces an active scene through a type-erased factory with optional transitions.
	bool ReEnterFactory(
		std::string_view scene_tag,
		SceneFactory scene_factory,
		std::unique_ptr<SceneTransition> transition_out,
		std::unique_ptr<SceneTransition> transition_in
	) {
		const auto scene_tag_hash{
			Hash(scene_tag)
		};

		if (!CanIssueCommands(scene_tag_hash) ||
			!HasScene(scene_tag_hash)) {
			return false;
		}

		PushCommand(
			CommandType::ReEnter,
			std::string{ scene_tag },
			scene_tag_hash,
			SceneTransitionPriority{
				std::numeric_limits<
					std::size_t
				>::max()
			},
			std::move(scene_factory),
			std::move(transition_out),
			std::move(transition_in)
		);

		return true;
	}

	/// @brief Exits a scene with a type-erased optional transition.
	bool Exit(
		std::string_view scene_tag, std::unique_ptr<SceneTransition> transition_out,
		SceneTransitionPriority priority = SceneTransitionPriority{}
	) {
		const auto scene_tag_hash{ Hash(scene_tag) };
		if (!CanIssueCommands(scene_tag_hash) || !HasScene(scene_tag_hash)) {
			return false;
		}

		PushCommand(
			CommandType::Exit, std::string{ scene_tag }, scene_tag_hash, priority, nullptr,
			std::move(transition_out), nullptr
		);
		return true;
	}

	/// @brief Exits one scene and enters another through a type-erased factory.
	bool TransitionFactory(
		std::string_view from_scene_tag, std::string_view to_scene_tag,
		SceneFactory scene_factory, std::unique_ptr<SceneTransition> transition_out,
		std::unique_ptr<SceneTransition> transition_in,
		SceneTransitionPriority priority = SceneTransitionPriority{}
	) {
		if (from_scene_tag == to_scene_tag) {
			return ReEnterFactory(
				to_scene_tag, std::move(scene_factory), std::move(transition_out),
				std::move(transition_in)
			);
		}

		const bool exited{ Exit(from_scene_tag, std::move(transition_out), priority) };
		const bool entered{ EnterFactory(
			to_scene_tag, std::move(scene_factory), std::move(transition_in), priority
		) };
		return exited || entered;
	}

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

	template <SceneType T, SceneTransitionType TransitionOut, SceneTransitionType TransitionIn, typename... TArgs>
		requires std::constructible_from<T, TArgs...>
	bool ReEnter(
		std::string_view scene_tag,
		SceneTransitionPair<
			TransitionOut,
			TransitionIn
		>&& transition,
		TArgs&&... constructor_args
	) {
		const auto scene_tag_hash{
			Hash(scene_tag)
		};

		if (!CanIssueCommands(scene_tag_hash) ||
			!HasScene(scene_tag_hash)) {
			return false;
		}

		std::unique_ptr<
			SceneTransition
		> transition_out_ptr;

		std::unique_ptr<
			SceneTransition
		> transition_in_ptr;

		if constexpr (
			!std::same_as<
				std::decay_t<TransitionOut>,
				NoTransition
			>
		) {
			transition_out_ptr =
				std::make_unique<
					std::decay_t<
						TransitionOut
					>
				>(
					std::move(
						transition.out
					)
				);
		}

		if constexpr (
			!std::same_as<
				std::decay_t<TransitionIn>,
				NoTransition
			>
		) {
			transition_in_ptr =
				std::make_unique<
					std::decay_t<
						TransitionIn
					>
				>(
					std::move(
						transition.in
					)
				);
		}

		PushCommand(
			CommandType::ReEnter,
			std::string{ scene_tag },
			scene_tag_hash,
			SceneTransitionPriority{
				std::numeric_limits<
					std::size_t
				>::max()
			},
			GetFactory<T>(
				std::forward<TArgs>(
					constructor_args
				)...
			),
			std::move(
				transition_out_ptr
			),
			std::move(
				transition_in_ptr
			)
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
		auto arguments{
			std::make_shared<std::tuple<std::decay_t<TArgs>...>>(
				std::forward<TArgs>(constructor_args)...
			)
		};

		SceneFactory::Construct construct = [arguments](
			Application& app, SceneData&& scene_data
		) -> std::unique_ptr<Scene> {
			auto scene{ std::apply(
				[](const auto&... args) { return std::make_unique<T>(args...); },
				*arguments
			) };
			scene_data.registered_type = GetRegisteredSceneType<T>();
			scene->Init(app, std::move(scene_data));
			return scene;
		};

		SceneFactory::Preload preload = [arguments](Application&) {
			auto scene{ std::apply(
				[](const auto&... args) { return T{ args... }; },
				*arguments
			) };
			AssetPreloadContext context;
			scene.OnPreload(context);
			for (const auto& key : scene.GetExplicitAssetDependencies()) {
				context.Add(key);
			}
			return context.GetDependencies();
		};

		return SceneFactory{ std::move(construct), std::move(preload) };
	}

	void PreUpdate();
	void OnEvent();
	void Update(Application& app, secondsf dt);
	void Draw(DrawContext& ctx) const;

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
	void ApplyLoadedCommand(
		Application& app,
		Command command,
		impl::AssetLoadTicket ticket
	);
	void UpdatePendingLoads(Application& app);
	void UpdateTransitions(secondsf dt);
	void UpdateReEnteredSceneTagHashes();

	[[nodiscard]] std::size_t GenerateTempTagHash() const;

	[[nodiscard]] bool CanIssueCommands(std::size_t target_scene_tag_hash) const;

	std::vector<std::unique_ptr<Scene>> scenes_;

	std::vector<Command> commands_;

	struct PendingLoad {
		Command command;
		impl::AssetLoadTicket ticket;
	};

	std::vector<PendingLoad> pending_loads_;

	/// @brief Contains the scene tag hashes of currently re-entering scenes.
	std::vector<ReEnteringScene> reentering_scenes_;
};

} // namespace impl

class LocalSceneManager {
public:
	bool EnterFactory(
		std::string_view scene_tag, impl::SceneFactory scene_factory,
		std::unique_ptr<SceneTransition> transition_in,
		SceneTransitionPriority priority = SceneTransitionPriority{}
	) {
		return scene_manager_.EnterFactory(
			scene_tag, std::move(scene_factory), std::move(transition_in), priority
		);
	}

	bool Exit(
		std::string_view scene_tag, std::unique_ptr<SceneTransition> transition_out,
		SceneTransitionPriority priority = SceneTransitionPriority{}
	) {
		return scene_manager_.Exit(scene_tag, std::move(transition_out), priority);
	}

	bool ReEnterFactory(
		std::string_view scene_tag,
		impl::SceneFactory scene_factory,
		std::unique_ptr<SceneTransition>
			transition_out,
		std::unique_ptr<SceneTransition>
			transition_in
	) {
		if (!CanIssueCommands()) {
			return false;
		}

		return scene_manager_.ReEnterFactory(
			scene_tag,
			std::move(scene_factory),
			std::move(transition_out),
			std::move(transition_in)
		);
	}

	bool TransitionFactory(
		std::string_view from_scene_tag,
		std::string_view to_scene_tag,
		impl::SceneFactory scene_factory,
		std::unique_ptr<SceneTransition>
			transition_out,
		std::unique_ptr<SceneTransition>
			transition_in,
		SceneTransitionPriority priority =
			SceneTransitionPriority{}
	) {
		if (!CanIssueCommands()) {
			return false;
		}

		return scene_manager_.TransitionFactory(
			from_scene_tag,
			to_scene_tag,
			std::move(scene_factory),
			std::move(transition_out),
			std::move(transition_in),
			priority
		);
	}

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

		PTGN_ASSERT(scene_);

		return Transition<ToScene>(
			scene_->GetTag(), scene_tag, std::move(transition), priority,
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

	LocalSceneManager() = delete;
	explicit LocalSceneManager(impl::SceneManager& scene_manager, Scene& scene);
	~LocalSceneManager() noexcept							   = default;
	LocalSceneManager(const LocalSceneManager&)				   = delete;
	LocalSceneManager& operator=(const LocalSceneManager&)	   = delete;
	LocalSceneManager(LocalSceneManager&&) noexcept			   = delete;
	LocalSceneManager& operator=(LocalSceneManager&&) noexcept = delete;

	void Rebind(Scene& scene);

	[[nodiscard]] bool CanIssueCommands() const;

	impl::SceneManager& scene_manager_;
	Scene* scene_{ nullptr };
};

} // namespace ptgn
