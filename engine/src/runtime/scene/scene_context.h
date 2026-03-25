#pragma once

#include <concepts>
#include <functional>
#include <limits>
#include <memory>
#include <string_view>
#include <vector>

#include "core/assert.h"
#include "core/event/dispatcher.h"
#include "core/time/time.h"
#include "core/util/hash.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/render_context.h"
#include "runtime/physics/collision_handler.h"
#include "runtime/physics/physics.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_command.h"
#include "runtime/scene/scene_input.h"
#include "runtime/scene/scene_manager.h"
#include "runtime/scene/scene_transition.h"
#include "tools/debug/debug_system.h"

namespace ptgn {

class Renderer;
class RenderTarget;
class Application;
class FontSystem;
class AssetManager;
class Scene;
class EventHandler;
class Window;
class AudioSystem;

class SceneEventHandler {
public:
	explicit SceneEventHandler(Scene& scene);

	void Emit(EventDispatcher d);

private:
	Scene& scene_;
};

class LocalSceneManager {
public:
	template <SceneType T, SceneTransitionType TransitionIn, typename... TArgs>
		requires std::constructible_from<T, TArgs...>
	bool Enter(
		std::string_view scene_key, TransitionIn&& transition_in, ScenePriority priority,
		TArgs&&... constructor_args
	) {
		return Enter<T>(
			Hash(scene_key), std::forward<TransitionIn>(transition_in), priority,
			std::forward<TArgs>(constructor_args)...
		);
	}

	template <SceneType T, SceneTransitionType TransitionIn, typename... TArgs>
		requires std::constructible_from<T, TArgs...>
	bool Enter(
		std::string_view scene_key, TransitionIn&& transition_in, TArgs&&... constructor_args
	) {
		return Enter<T>(
			scene_key, std::forward<TransitionIn>(transition_in), ScenePriority{},
			std::forward<TArgs>(constructor_args)...
		);
	}

	template <SceneType T, typename... TArgs>
		requires std::constructible_from<T, TArgs...>
	bool Enter(std::string_view scene_key, ScenePriority priority, TArgs&&... constructor_args) {
		return Enter<T>(
			scene_key, NoTransition{}, priority, std::forward<TArgs>(constructor_args)...
		);
	}

	template <SceneType T, typename... TArgs>
		requires std::constructible_from<T, TArgs...>
	bool Enter(std::string_view scene_key, TArgs&&... constructor_args) {
		return Enter<T>(
			scene_key, NoTransition{}, ScenePriority{}, std::forward<TArgs>(constructor_args)...
		);
	}

	template <SceneTransitionType TransitionOut>
	bool Exit(
		std::string_view scene_key, TransitionOut&& transition_out,
		ScenePriority priority = ScenePriority{ 0 }
	) {
		return Exit(Hash(scene_key), std::forward<TransitionOut>(transition_out), priority);
	}

	bool Exit(std::string_view scene_key, ScenePriority priority = ScenePriority{ 0 }) {
		return Exit(scene_key, NoTransition{}, priority);
	}

	template <
		SceneType T, SceneTransitionType TransitionOut, SceneTransitionType TransitionIn,
		typename... TArgs>
		requires std::constructible_from<T, TArgs...>
	bool ReEnter(
		std::string_view scene_key, TransitionOut&& transition_out, TransitionIn&& transition_in,
		TArgs&&... constructor_args
	) {
		return ReEnter<T>(
			Hash(scene_key), std::forward<TransitionOut>(transition_out),
			std::forward<TransitionIn>(transition_in), std::forward<TArgs>(constructor_args)...
		);
	}

	template <SceneType T, SceneTransitionType TransitionOut, typename... TArgs>
		requires std::constructible_from<T, TArgs...>
	bool ReEnter(
		std::string_view scene_key, TransitionOut&& transition_out, std::initializer_list<int>,
		TArgs&&... constructor_args
	) {
		return ReEnter<T>(
			scene_key, std::forward<TransitionOut>(transition_out), NoTransition{},
			std::forward<TArgs>(constructor_args)...
		);
	}

	template <SceneType T, SceneTransitionType TransitionIn, typename... TArgs>
		requires std::constructible_from<T, TArgs...>
	bool ReEnter(
		std::string_view scene_key, std::initializer_list<int>, TransitionIn&& transition_in,
		TArgs&&... constructor_args
	) {
		return ReEnter<T>(
			scene_key, NoTransition{}, std::forward<TransitionIn>(transition_in),
			std::forward<TArgs>(constructor_args)...
		);
	}

	template <SceneType T, typename... TArgs>
		requires std::constructible_from<T, TArgs...>
	bool ReEnter(std::string_view scene_key, TArgs&&... constructor_args) {
		return ReEnter<T>(
			scene_key, NoTransition{}, NoTransition{}, std::forward<TArgs>(constructor_args)...
		);
	}

	template <
		SceneType ToScene, SceneTransitionType TransitionOut, SceneTransitionType TransitionIn,
		typename... TArgs>
		requires std::constructible_from<ToScene, TArgs...>
	bool Transition(
		std::string_view from_scene_key, std::string_view to_scene_key,
		TransitionOut&& transition_out, TransitionIn&& transition_in, ScenePriority priority,
		TArgs&&... to_scene_constructor_args
	) {
		return Transition<ToScene>(
			Hash(from_scene_key), Hash(to_scene_key), std::forward<TransitionOut>(transition_out),
			std::forward<TransitionIn>(transition_in), priority,
			std::forward<TArgs>(to_scene_constructor_args)...
		);
	}

	template <
		SceneType ToScene, SceneTransitionType TransitionOut, SceneTransitionType TransitionIn,
		typename... TArgs>
		requires std::constructible_from<ToScene, TArgs...>
	bool Transition(
		std::string_view from_scene_key, std::string_view to_scene_key,
		TransitionOut&& transition_out, TransitionIn&& transition_in,
		TArgs&&... to_scene_constructor_args
	) {
		return Transition<ToScene>(
			from_scene_key, to_scene_key, std::forward<TransitionOut>(transition_out),
			std::forward<TransitionIn>(transition_in), ScenePriority{},
			std::forward<TArgs>(to_scene_constructor_args)...
		);
	}

	template <SceneType ToScene, SceneTransitionType TransitionIn, typename... TArgs>
		requires std::constructible_from<ToScene, TArgs...>
	bool Transition(
		std::string_view from_scene_key, std::string_view to_scene_key, std::initializer_list<int>,
		TransitionIn&& transition_in, ScenePriority priority, TArgs&&... to_scene_constructor_args
	) {
		return Transition<ToScene>(
			from_scene_key, to_scene_key, NoTransition{}, std::forward<TransitionIn>(transition_in),
			priority, std::forward<TArgs>(to_scene_constructor_args)...
		);
	}

	template <SceneType ToScene, SceneTransitionType TransitionIn, typename... TArgs>
		requires std::constructible_from<ToScene, TArgs...>
	bool Transition(
		std::string_view from_scene_key, std::string_view to_scene_key, std::initializer_list<int>,
		TransitionIn&& transition_in, TArgs&&... to_scene_constructor_args
	) {
		return Transition<ToScene>(
			from_scene_key, to_scene_key, NoTransition{}, std::forward<TransitionIn>(transition_in),
			ScenePriority{}, std::forward<TArgs>(to_scene_constructor_args)...
		);
	}

	template <SceneType ToScene, SceneTransitionType TransitionOut, typename... TArgs>
		requires std::constructible_from<ToScene, TArgs...>
	bool Transition(
		std::string_view from_scene_key, std::string_view to_scene_key,
		TransitionOut&& transition_out, std::initializer_list<int>, ScenePriority priority,
		TArgs&&... to_scene_constructor_args
	) {
		return Transition<ToScene>(
			from_scene_key, to_scene_key, std::forward<TransitionOut>(transition_out),
			NoTransition{}, priority, std::forward<TArgs>(to_scene_constructor_args)...
		);
	}

	template <SceneType ToScene, SceneTransitionType TransitionOut, typename... TArgs>
		requires std::constructible_from<ToScene, TArgs...>
	bool Transition(
		std::string_view from_scene_key, std::string_view to_scene_key,
		TransitionOut&& transition_out, std::initializer_list<int>,
		TArgs&&... to_scene_constructor_args
	) {
		return Transition<ToScene>(
			from_scene_key, to_scene_key, std::forward<TransitionOut>(transition_out),
			NoTransition{}, ScenePriority{}, std::forward<TArgs>(to_scene_constructor_args)...
		);
	}

	template <SceneType ToScene, typename... TArgs>
		requires std::constructible_from<ToScene, TArgs...>
	bool Transition(
		std::string_view from_scene_key, std::string_view to_scene_key, ScenePriority priority,
		TArgs&&... to_scene_constructor_args
	) {
		return Transition<ToScene>(
			from_scene_key, to_scene_key, NoTransition{}, NoTransition{}, priority,
			std::forward<TArgs>(to_scene_constructor_args)...
		);
	}

	template <SceneType ToScene, typename... TArgs>
		requires std::constructible_from<ToScene, TArgs...>
	bool Transition(
		std::string_view from_scene_key, std::string_view to_scene_key,
		TArgs&&... to_scene_constructor_args
	) {
		return Transition<ToScene>(
			from_scene_key, to_scene_key, NoTransition{}, NoTransition{}, ScenePriority{},
			std::forward<TArgs>(to_scene_constructor_args)...
		);
	}

	template <
		SceneType ToScene, SceneTransitionType TransitionOut, SceneTransitionType TransitionIn,
		typename... TArgs>
		requires std::constructible_from<ToScene, TArgs...>
	bool Switch(
		std::string_view scene_key, TransitionOut&& transition_out, TransitionIn&& transition_in,
		ScenePriority priority, TArgs&&... constructor_args
	) {
		return Transition<ToScene>(
			scene_.key_, Hash(scene_key), std::forward<TransitionOut>(transition_out),
			std::forward<TransitionIn>(transition_in), priority,
			std::forward<TArgs>(constructor_args)...
		);
	}

	template <
		SceneType ToScene, SceneTransitionType TransitionOut, SceneTransitionType TransitionIn,
		typename... TArgs>
		requires std::constructible_from<ToScene, TArgs...>
	bool Switch(
		std::string_view scene_key, TransitionOut&& transition_out, TransitionIn&& transition_in,
		TArgs&&... constructor_args
	) {
		return Switch<ToScene>(
			scene_key, std::forward<TransitionOut>(transition_out),
			std::forward<TransitionIn>(transition_in), ScenePriority{},
			std::forward<TArgs>(constructor_args)...
		);
	}

	template <SceneType ToScene, SceneTransitionType TransitionIn, typename... TArgs>
		requires std::constructible_from<ToScene, TArgs...>
	bool Switch(
		std::string_view scene_key, std::initializer_list<int>, TransitionIn&& transition_in,
		ScenePriority priority, TArgs&&... constructor_args
	) {
		return Switch<ToScene>(
			scene_key, NoTransition{}, std::forward<TransitionIn>(transition_in), priority,
			std::forward<TArgs>(constructor_args)...
		);
	}

	template <SceneType ToScene, SceneTransitionType TransitionIn, typename... TArgs>
		requires std::constructible_from<ToScene, TArgs...>
	bool Switch(
		std::string_view scene_key, std::initializer_list<int>, TransitionIn&& transition_in,
		TArgs&&... constructor_args
	) {
		return Switch<ToScene>(
			scene_key, NoTransition{}, std::forward<TransitionIn>(transition_in), ScenePriority{},
			std::forward<TArgs>(constructor_args)...
		);
	}

	template <SceneType ToScene, SceneTransitionType TransitionOut, typename... TArgs>
		requires std::constructible_from<ToScene, TArgs...>
	bool Switch(
		std::string_view scene_key, TransitionOut&& transition_out, std::initializer_list<int>,
		ScenePriority priority, TArgs&&... constructor_args
	) {
		return Switch<ToScene>(
			scene_key, std::forward<TransitionOut>(transition_out), NoTransition{}, priority,
			std::forward<TArgs>(constructor_args)...
		);
	}

	template <SceneType ToScene, SceneTransitionType TransitionOut, typename... TArgs>
		requires std::constructible_from<ToScene, TArgs...>
	bool Switch(
		std::string_view scene_key, TransitionOut&& transition_out, std::initializer_list<int>,
		TArgs&&... constructor_args
	) {
		return Switch<ToScene>(
			scene_key, std::forward<TransitionOut>(transition_out), NoTransition{}, ScenePriority{},
			std::forward<TArgs>(constructor_args)...
		);
	}

	template <SceneType ToScene, typename... TArgs>
		requires std::constructible_from<ToScene, TArgs...>
	bool Switch(std::string_view scene_key, ScenePriority priority, TArgs&&... constructor_args) {
		return Switch<ToScene>(
			scene_key, NoTransition{}, NoTransition{}, priority,
			std::forward<TArgs>(constructor_args)...
		);
	}

	template <SceneType ToScene, typename... TArgs>
		requires std::constructible_from<ToScene, TArgs...>
	bool Switch(std::string_view scene_key, TArgs&&... constructor_args) {
		return Switch<ToScene>(
			scene_key, NoTransition{}, NoTransition{}, ScenePriority{},
			std::forward<TArgs>(constructor_args)...
		);
	}

private:
	friend class SceneContext;

	explicit LocalSceneManager(SceneManager& scene_manager, Scene& scene);

	[[nodiscard]] bool CanIssueCommands(std::size_t target_key) const;

	template <SceneType T, typename... TArgs>
	std::function<std::unique_ptr<Scene>()> GetInitFunction(TArgs&&... constructor_args);

	template <
		SceneType T, SceneTransitionType TransitionOut, SceneTransitionType TransitionIn,
		typename... TArgs>
		requires std::constructible_from<T, TArgs...>
	bool ReEnter(
		std::size_t scene_key_hash, TransitionOut&& transition_out, TransitionIn&& transition_in,
		TArgs&&... constructor_args
	) {
		if (!CanIssueCommands(scene_key_hash)) {
			return false;
		}

		PTGN_ASSERT(
			scene_manager_.Has(scene_key_hash),
			"Cannot re-enter a scene key which has not been entered"
		);

		std::unique_ptr<SceneTransition> transition_out_ptr;
		std::unique_ptr<SceneTransition> transition_in_ptr;

		if constexpr (!std::same_as<std::decay_t<TransitionOut>, NoTransition>) {
			transition_out_ptr = std::make_unique<std::decay_t<TransitionOut>>(
				std::forward<TransitionOut>(transition_out)
			);
		}

		if constexpr (!std::same_as<std::decay_t<TransitionIn>, NoTransition>) {
			transition_in_ptr = std::make_unique<std::decay_t<TransitionIn>>(
				std::forward<TransitionIn>(transition_in)
			);
		}

		scene_manager_.commands_.emplace_back(
			impl::SceneCommandType::ReEnter, scene_.key_, scene_key_hash,
			ScenePriority{ std::numeric_limits<std::size_t>::max() },
			GetInitFunction<T>(std::forward<TArgs>(constructor_args)...),
			std::move(transition_out_ptr), std::move(transition_in_ptr)
		);

		return true;
	}

	template <SceneType T, SceneTransitionType TransitionIn, typename... TArgs>
		requires std::constructible_from<T, TArgs...>
	bool Enter(
		std::size_t scene_key_hash, TransitionIn&& transition_in, ScenePriority priority,
		TArgs&&... constructor_args
	) {
		if (!CanIssueCommands(scene_key_hash)) {
			return false;
		}

		if (scene_manager_.Has(scene_key_hash)) {
			return ReEnter<T>(
				scene_key_hash, NoTransition{}, std::forward<TransitionIn>(transition_in),
				std::forward<TArgs>(constructor_args)...
			);
		}

		std::unique_ptr<SceneTransition> transition_in_ptr;

		if constexpr (!std::same_as<std::decay_t<TransitionIn>, NoTransition>) {
			transition_in_ptr = std::make_unique<std::decay_t<TransitionIn>>(
				std::forward<TransitionIn>(transition_in)
			);
		}

		scene_manager_.commands_.emplace_back(
			impl::SceneCommandType::Enter, scene_.key_, scene_key_hash, priority,
			GetInitFunction<T>(std::forward<TArgs>(constructor_args)...), nullptr,
			std::move(transition_in_ptr)
		);

		return true;
	}

	template <SceneTransitionType TransitionOut>
	bool Exit(
		std::size_t scene_key_hash, TransitionOut&& transition_out,
		ScenePriority priority = ScenePriority{ 0 }
	) {
		if (!CanIssueCommands(scene_key_hash)) {
			return false;
		}

		if (!scene_manager_.Has(scene_key_hash)) {
			return false;
		}

		std::unique_ptr<SceneTransition> transition_out_ptr;

		if constexpr (!std::same_as<std::decay_t<TransitionOut>, NoTransition>) {
			transition_out_ptr = std::make_unique<std::decay_t<TransitionOut>>(
				std::forward<TransitionOut>(transition_out)
			);
		}

		scene_manager_.commands_.emplace_back(
			impl::SceneCommandType::Exit, scene_.key_, scene_key_hash, priority, nullptr,
			std::move(transition_out_ptr), nullptr
		);

		return true;
	}

	template <
		SceneType ToScene, SceneTransitionType TransitionOut, SceneTransitionType TransitionIn,
		typename... TArgs>
		requires std::constructible_from<ToScene, TArgs...>
	bool Transition(
		std::size_t from_scene_key_hash, std::size_t to_scene_key_hash,
		TransitionOut&& transition_out, TransitionIn&& transition_in, ScenePriority priority,
		TArgs&&... to_scene_constructor_args
	) {
		if (from_scene_key_hash == to_scene_key_hash) {
			return ReEnter<ToScene>(
				to_scene_key_hash, std::forward<TransitionOut>(transition_out),
				std::forward<TransitionIn>(transition_in),
				std::forward<TArgs>(to_scene_constructor_args)...
			);
		}

		bool exited{
			Exit(from_scene_key_hash, std::forward<TransitionOut>(transition_out), priority)
		};
		bool entered{ Enter<ToScene>(
			to_scene_key_hash, std::forward<TransitionIn>(transition_in), priority,
			std::forward<TArgs>(to_scene_constructor_args)...
		) };
		return exited || entered;
	}

	SceneManager& scene_manager_;
	Scene& scene_;
};

class SceneContext {
public:
	SceneContext() = delete;
	explicit SceneContext(Application& app, Scene& parent_scene);
	~SceneContext() noexcept;
	SceneContext(const SceneContext&)				 = delete;
	SceneContext& operator=(const SceneContext&)	 = delete;
	SceneContext(SceneContext&&) noexcept			 = default;
	SceneContext& operator=(SceneContext&&) noexcept = default;

	EventHandler& global_event;
	Window& window;
	AssetManager& asset;
	FontSystem& font;
	AudioSystem& audio;

	LocalSceneManager scene;
	RenderContext renderer;
	DebugContext debug;
	SceneEventHandler event;
	SceneInput input;
	Physics physics;
	CollisionHandler collision;

	/// @brief The default camera used by all objects in the scene. By default it resizes to the
	/// game size.
	Camera camera;

	/// @brief Terminates the main application loop.
	void Stop();

	/// @brief Returns the delta time of the current frame.
	secondsf dt() const;

	/// @brief Returns the time elapsed since the Application instance was constructed.
	[[nodiscard]] milliseconds TimeSinceStart() const;

	/// @brief Returns whether the application is currently running.
	bool IsRunning() const;

	/// @brief Returns the total number of frames that the application has run for.
	std::size_t GetFrameCount() const;

private:
	friend class RenderTarget;
	friend class Scene;
	friend class LocalSceneManager;

	/// @brief An optional secondary fixed camera for the scene. By default it resizes to the game
	/// size.
	Camera fixed_camera_;

	Renderer& global_renderer_;
	Application& app_;
};

template <SceneType T, typename... TArgs>
std::function<std::unique_ptr<Scene>()> LocalSceneManager::GetInitFunction(
	TArgs&&... constructor_args
) {
	return [app = &scene_.ctx().app_, constructor_args...]() -> std::unique_ptr<Scene> {
		auto scene{ std::make_unique<T>(constructor_args...) };
		scene->Init(*app);
		return scene;
	};
}

} // namespace ptgn