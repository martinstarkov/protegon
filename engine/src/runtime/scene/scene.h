#pragma once

#include <concepts>
#include <functional>
#include <limits>
#include <memory>
#include <string_view>
#include <type_traits>
#include <vector>

#include "core/assert.h"
#include "core/event/dispatcher.h"
#include "core/time/time.h"
#include "core/util/hash.h"
#include "ecs/ecs.h"
#include "renderer/primitives/color.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/manager.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/render_context.h"
#include "runtime/graphics/render_target_component.h"
#include "runtime/physics/collision_handler.h"
#include "runtime/physics/physics.h"
#include "runtime/scene/scene_command.h"
#include "runtime/scene/scene_input.h"
#include "runtime/scene/scene_manager.h"
#include "runtime/scene/scene_state.h"
#include "runtime/scene/scene_transition.h"
#include "runtime/scene/scene_view.h"
#include "serialization/json/archiver.h"
#include "serialization/json/fwd.h"
#include "tools/debug/debug_system.h"

namespace ptgn {

class Application;
class Scene;
class SceneManager;
class SceneTransition;
class SceneEventHandler;
class LocalSceneManager;
class Renderer;
class RenderTarget;
class FontSystem;
class AssetManager;
class EventHandler;
class Window;
class AudioSystem;

template <typename T>
concept SceneType = std::derived_from<T, Scene>;

class SceneEventHandler {
public:
	explicit SceneEventHandler(Scene& scene);

	void Emit(EventDispatcher d);

private:
	Scene& scene_;
};

template <typename TComponent>
struct SceneHook {
	Scene& scene;
	ecs::Hook<void, ecs::impl::EntityHandle<JsonArchiver>>& hook;

	template <auto Member>
	void Connect();
};

class LocalSceneManager {
public:
	template <SceneType T, SceneTransitionType TransitionIn, typename... TArgs>
		requires std::constructible_from<T, TArgs...>
	bool Enter(
		std::string_view scene_key, TransitionIn&& transition_in, SceneTransitionPriority priority,
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
			scene_key, std::forward<TransitionIn>(transition_in), SceneTransitionPriority{},
			std::forward<TArgs>(constructor_args)...
		);
	}

	template <SceneType T, typename... TArgs>
		requires std::constructible_from<T, TArgs...>
	bool Enter(
		std::string_view scene_key, SceneTransitionPriority priority, TArgs&&... constructor_args
	) {
		return Enter<T>(
			scene_key, NoTransition{}, priority, std::forward<TArgs>(constructor_args)...
		);
	}

	template <SceneType T, typename... TArgs>
		requires std::constructible_from<T, TArgs...>
	bool Enter(std::string_view scene_key, TArgs&&... constructor_args) {
		return Enter<T>(
			scene_key, NoTransition{}, SceneTransitionPriority{},
			std::forward<TArgs>(constructor_args)...
		);
	}

	template <SceneTransitionType TransitionOut>
	bool Exit(
		std::string_view scene_key, TransitionOut&& transition_out,
		SceneTransitionPriority priority = SceneTransitionPriority{ 0 }
	) {
		return Exit(Hash(scene_key), std::forward<TransitionOut>(transition_out), priority);
	}

	bool Exit(
		std::string_view scene_key, SceneTransitionPriority priority = SceneTransitionPriority{ 0 }
	) {
		return Exit(scene_key, NoTransition{}, priority);
	}

	template <
		SceneType T, SceneTransitionType TransitionOut, SceneTransitionType TransitionIn,
		typename... TArgs>
		requires std::constructible_from<T, TArgs...>
	bool ReEnter(
		std::string_view scene_key, SceneTransitionPair<TransitionOut, TransitionIn>&& transition,
		TArgs&&... constructor_args
	) {
		return ReEnter<T>(
			Hash(scene_key), std::move(transition), std::forward<TArgs>(constructor_args)...
		);
	}

	template <SceneType T, typename... TArgs>
		requires std::constructible_from<T, TArgs...>
	bool ReEnter(std::string_view scene_key, TArgs&&... constructor_args) {
		return ReEnter<T>(
			scene_key, SceneTransitionPair{}, std::forward<TArgs>(constructor_args)...
		);
	}

	template <
		SceneType ToScene, SceneTransitionType TransitionOut, SceneTransitionType TransitionIn,
		typename... TArgs>
		requires std::constructible_from<ToScene, TArgs...>
	bool Transition(
		std::string_view from_scene_key, std::string_view to_scene_key,
		SceneTransitionPair<TransitionOut, TransitionIn>&& transition,
		SceneTransitionPriority priority, TArgs&&... to_scene_constructor_args
	) {
		return Transition<ToScene>(
			Hash(from_scene_key), Hash(to_scene_key), std::move(transition), priority,
			std::forward<TArgs>(to_scene_constructor_args)...
		);
	}

	template <
		SceneType ToScene, SceneTransitionType TransitionOut, SceneTransitionType TransitionIn,
		typename... TArgs>
		requires std::constructible_from<ToScene, TArgs...>
	bool Transition(
		std::string_view from_scene_key, std::string_view to_scene_key,
		SceneTransitionPair<TransitionOut, TransitionIn>&& transition,
		TArgs&&... to_scene_constructor_args
	) {
		return Transition<ToScene>(
			from_scene_key, to_scene_key, std::move(transition), SceneTransitionPriority{},
			std::forward<TArgs>(to_scene_constructor_args)...
		);
	}

	template <SceneType ToScene, typename... TArgs>
		requires std::constructible_from<ToScene, TArgs...>
	bool Transition(
		std::string_view from_scene_key, std::string_view to_scene_key,
		SceneTransitionPriority priority, TArgs&&... to_scene_constructor_args
	) {
		return Transition<ToScene>(
			from_scene_key, to_scene_key, SceneTransitionPair{}, priority,
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
			from_scene_key, to_scene_key, SceneTransitionPair{}, SceneTransitionPriority{},
			std::forward<TArgs>(to_scene_constructor_args)...
		);
	}

	template <
		SceneType ToScene, SceneTransitionType TransitionOut, SceneTransitionType TransitionIn,
		typename... TArgs>
		requires std::constructible_from<ToScene, TArgs...>
	bool Switch(
		std::string_view scene_key, SceneTransitionPair<TransitionOut, TransitionIn>&& transition,
		SceneTransitionPriority priority, TArgs&&... constructor_args
	);

	template <
		SceneType ToScene, SceneTransitionType TransitionOut, SceneTransitionType TransitionIn,
		typename... TArgs>
		requires std::constructible_from<ToScene, TArgs...>
	bool Switch(
		std::string_view scene_key, SceneTransitionPair<TransitionOut, TransitionIn>&& transition,
		TArgs&&... constructor_args
	) {
		return Switch<ToScene>(
			scene_key, std::move(transition), SceneTransitionPriority{},
			std::forward<TArgs>(constructor_args)...
		);
	}

	template <SceneType ToScene, typename... TArgs>
		requires std::constructible_from<ToScene, TArgs...>
	bool Switch(
		std::string_view scene_key, SceneTransitionPriority priority, TArgs&&... constructor_args
	) {
		return Switch<ToScene>(
			scene_key, SceneTransitionPair{}, priority, std::forward<TArgs>(constructor_args)...
		);
	}

	template <SceneType ToScene, typename... TArgs>
		requires std::constructible_from<ToScene, TArgs...>
	bool Switch(std::string_view scene_key, TArgs&&... constructor_args) {
		return Switch<ToScene>(
			scene_key, SceneTransitionPair{}, SceneTransitionPriority{},
			std::forward<TArgs>(constructor_args)...
		);
	}

private:
	friend class Scene;
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
		std::size_t scene_key_hash, SceneTransitionPair<TransitionOut, TransitionIn>&& transition,
		TArgs&&... constructor_args
	);

	template <SceneType T, SceneTransitionType TransitionIn, typename... TArgs>
		requires std::constructible_from<T, TArgs...>
	bool Enter(
		std::size_t scene_key_hash, TransitionIn&& transition_in, SceneTransitionPriority priority,
		TArgs&&... constructor_args
	);

	template <SceneTransitionType TransitionOut>
	bool Exit(
		std::size_t scene_key_hash, TransitionOut&& transition_out,
		SceneTransitionPriority priority = SceneTransitionPriority{ 0 }
	);

	template <
		SceneType ToScene, SceneTransitionType TransitionOut, SceneTransitionType TransitionIn,
		typename... TArgs>
		requires std::constructible_from<ToScene, TArgs...>
	bool Transition(
		std::size_t from_scene_key_hash, std::size_t to_scene_key_hash,
		SceneTransitionPair<TransitionOut, TransitionIn>&& transition,
		SceneTransitionPriority priority, TArgs&&... to_scene_constructor_args
	) {
		if (from_scene_key_hash == to_scene_key_hash) {
			return ReEnter<ToScene>(
				to_scene_key_hash, std::move(transition),
				std::forward<TArgs>(to_scene_constructor_args)...
			);
		}

		bool exited{ Exit(from_scene_key_hash, std::move(transition.out), priority) };
		bool entered{ Enter<ToScene>(
			to_scene_key_hash, std::move(transition.in), priority,
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
	SceneContext& operator=(SceneContext&&) noexcept = delete;

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

class Scene {
public:
	Scene()			 = default;
	virtual ~Scene() = default;

	/// @brief Called when the scene is added to active scenes.
	virtual void OnEnter() {
		/* user implementation */
	}

	/// @brief Called once per frame for each active scene.
	virtual void OnUpdate() {
		/* user implementation */
	}

	/// @brief Called when the scene is removed from active scenes.
	virtual void OnExit() {
		/* user implementation */
	}

	/// @brief Called an event is emitted by the event handler.
	virtual void OnEvent(EventDispatcher) {
		/* user implementation */
	}

	template <
		SceneType T, SceneTransitionType TransitionOut, SceneTransitionType TransitionIn,
		typename... TArgs>
		requires std::constructible_from<T, TArgs...>
	bool ReEnter(
		SceneTransitionPair<TransitionOut, TransitionIn>&& transition, TArgs&&... constructor_args
	) {
		return ctx().scene.ReEnter<T>(
			key_, std::move(transition), std::forward<TArgs>(constructor_args)...
		);
	}

	template <SceneType T, typename... TArgs>
		requires std::constructible_from<T, TArgs...>
	bool ReEnter(TArgs&&... constructor_args) {
		return ctx().scene.ReEnter<T>(
			key_, SceneTransitionPair{}, std::forward<TArgs>(constructor_args)...
		);
	}

	/// @brief Sets the background color of the scene. The default background color is transparent.
	void SetBackgroundColor(Color background_color);

	/// @return The background color of the scene.
	Color GetBackgroundColor() const;

	/// @return {} if no entity with the given uuid exists in the manager.
	Entity GetEntityByUUID(UUID uuid) const;

	/// @brief Make sure to call Refresh() after this function.
	Entity CreateEntity();

	/// @brief Creates an entity with a specific uuid.
	/// Make sure to call Refresh() after this function.
	Entity CreateEntity(UUID uuid);

	/// @brief Creates an entity from a json object.
	/// Make sure to call Refresh() after this function.
	Entity CreateEntity(const json& j);

	/// @brief Make sure to call Refresh() after this function.
	template <typename... Ts>
	Entity CopyEntity(Entity from) {
		auto entity{ manager_.CopyEntity<Ts...>(from) };
		entity.template Add<UUID>();
		return entity;
	}

	template <typename... Ts>
	void CopyEntity(const Entity& from, Entity& to) {
		manager_.CopyEntity<UUID>(from.entity_, to.entity_);
		manager_.CopyEntity<Ts...>(from.entity_, to.entity_);
	}

	auto Entities() {
		using EcsView = decltype(manager_.Entities());
		return SceneEntityRange<Scene, EcsView>{ this, manager_.Entities() };
	}

	auto Entities() const {
		using EcsView = decltype(manager_.Entities());
		return SceneEntityRange<const Scene, EcsView>{ this, manager_.Entities() };
	}

	template <typename... TComponents>
	auto EntitiesWith() {
		using EcsView = decltype(manager_.template EntitiesWith<TComponents...>());
		return SceneEntitiesWithRange<Scene, EcsView, TComponents...>{
			this, manager_.template EntitiesWith<TComponents...>()
		};
	}

	template <typename... TComponents>
	auto EntitiesWith() const {
		using EcsView = decltype(manager_.template EntitiesWith<TComponents...>());
		return SceneEntitiesWithRange<const Scene, EcsView, const TComponents...>{
			this, manager_.template EntitiesWith<TComponents...>()
		};
	}

	template <typename... TComponents>
	auto EntitiesWithout() {
		using EcsView = decltype(manager_.template EntitiesWithout<TComponents...>());
		return SceneEntityRange<Scene, EcsView>{
			this, manager_.template EntitiesWithout<TComponents...>()
		};
	}

	template <typename... TComponents>
	auto EntitiesWithout() const {
		using EcsView = decltype(manager_.template EntitiesWithout<TComponents...>());
		return SceneEntityRange<const Scene, EcsView>{
			this, manager_.template EntitiesWithout<TComponents...>()
		};
	}

	template <typename TComponent>
	auto OnConstruct() {
		return SceneHook<TComponent>{ *this, manager_.template OnConstruct<TComponent>() };
	}

	template <typename TComponent>
	auto OnDestruct() {
		return SceneHook<TComponent>{ *this, manager_.template OnDestruct<TComponent>() };
	}

	template <typename TComponent>
	auto OnUpdate() {
		return SceneHook<TComponent>{ *this, manager_.template OnUpdate<TComponent>() };
	}

	friend void to_json(json& j, const Scene& scene);
	friend void from_json(const json& j, Scene& scene);

	void Refresh();

	std::size_t GetEntityCount() const;

	RenderTarget GetRenderTarget() const;

	[[nodiscard]] const SceneContext& ctx() const;
	[[nodiscard]] SceneContext& ctx();

private:
	friend class SceneManager;
	friend class EventHandler;
	friend class Application;
	friend class FrameContext;
	friend class SceneInput;
	friend class LocalSceneManager;
	friend class SceneManager;
	friend class SceneEventHandler;
	template <typename TComponent>
	friend struct SceneHook;

	template <auto Member>
	void HookThunk(ecs::impl::EntityHandle<JsonArchiver> handle) {
		(this->*Member)(Entity{ handle, this });
	}

	void Init(Application& app);

	/// @brief Called by scene manager when a new scene is loaded and entered.
	void InternalEnter();
	void InternalExit();

	void InternalUpdate();
	void InternalDraw();
	void InternalEmit(EventDispatcher d);

	[[nodiscard]] bool IsTransitioning() const;

	std::unique_ptr<SceneContext> ctx_;
	Manager manager_;
	RenderTarget render_target_;

	std::size_t key_{ 0 };
	std::unique_ptr<SceneTransition> transition_;
	impl::SceneState state_{ impl::SceneState::Active };
};

template <typename TComponent>
template <auto Member>
void SceneHook<TComponent>::Connect() {
	hook.template Connect<Scene, &Scene::template HookThunk<Member>>(&scene);
}

template <SceneType T, typename... TArgs>
std::function<std::unique_ptr<Scene>()> LocalSceneManager::LocalSceneManager::GetInitFunction(
	TArgs&&... constructor_args
) {
	return [app = &scene_.ctx().app_, constructor_args...]() -> std::unique_ptr<Scene> {
		auto scene{ std::make_unique<T>(constructor_args...) };
		scene->Init(*app);
		return scene;
	};
}

template <
	SceneType T, SceneTransitionType TransitionOut, SceneTransitionType TransitionIn,
	typename... TArgs>
	requires std::constructible_from<T, TArgs...>
bool LocalSceneManager::ReEnter(
	std::size_t scene_key_hash, SceneTransitionPair<TransitionOut, TransitionIn>&& transition,
	TArgs&&... constructor_args
) {
	if (!CanIssueCommands(scene_key_hash)) {
		return false;
	}

	PTGN_ASSERT(
		scene_manager_.Has(scene_key_hash), "Cannot re-enter a scene key which has not been entered"
	);

	std::unique_ptr<SceneTransition> transition_out_ptr;
	std::unique_ptr<SceneTransition> transition_in_ptr;

	if constexpr (!std::same_as<std::decay_t<TransitionOut>, NoTransition>) {
		transition_out_ptr =
			std::make_unique<std::decay_t<TransitionOut>>(std::move(transition.out));
	}

	if constexpr (!std::same_as<std::decay_t<TransitionIn>, NoTransition>) {
		transition_in_ptr = std::make_unique<std::decay_t<TransitionIn>>(std::move(transition.in));
	}

	scene_manager_.commands_.emplace_back(
		impl::SceneCommandType::ReEnter, scene_.key_, scene_key_hash,
		SceneTransitionPriority{ std::numeric_limits<std::size_t>::max() },
		GetInitFunction<T>(std::forward<TArgs>(constructor_args)...), std::move(transition_out_ptr),
		std::move(transition_in_ptr)
	);

	return true;
}

template <SceneType T, SceneTransitionType TransitionIn, typename... TArgs>
	requires std::constructible_from<T, TArgs...>
bool LocalSceneManager::Enter(
	std::size_t scene_key_hash, TransitionIn&& transition_in, SceneTransitionPriority priority,
	TArgs&&... constructor_args
) {
	if (!CanIssueCommands(scene_key_hash)) {
		return false;
	}

	if (scene_manager_.Has(scene_key_hash)) {
		return ReEnter<T>(
			scene_key_hash, SceneTransitionPair{ {}, std::forward<TransitionIn>(transition_in) },
			std::forward<TArgs>(constructor_args)...
		);
	}

	std::unique_ptr<SceneTransition> transition_in_ptr;

	if constexpr (!std::same_as<std::decay_t<TransitionIn>, NoTransition>) {
		transition_in_ptr =
			std::make_unique<std::decay_t<TransitionIn>>(std::forward<TransitionIn>(transition_in));
	}

	scene_manager_.commands_.emplace_back(
		impl::SceneCommandType::Enter, scene_.key_, scene_key_hash, priority,
		GetInitFunction<T>(std::forward<TArgs>(constructor_args)...), nullptr,
		std::move(transition_in_ptr)
	);

	return true;
}

template <SceneTransitionType TransitionOut>
bool LocalSceneManager::Exit(
	std::size_t scene_key_hash, TransitionOut&& transition_out, SceneTransitionPriority priority
) {
	if (!CanIssueCommands(scene_key_hash)) {
		return false;
	}

	if (!scene_manager_.Has(scene_key_hash)) {
		return false;
	}

	std::unique_ptr<SceneTransition> transition_out_ptr;

	if constexpr (!std::same_as<std::decay_t<TransitionOut>, NoTransition>) {
		transition_out_ptr =
			std::make_unique<std::decay_t<TransitionOut>>(std::forward<TransitionOut>(transition_out
			));
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
bool LocalSceneManager::Switch(
	std::string_view scene_key, SceneTransitionPair<TransitionOut, TransitionIn>&& transition,
	SceneTransitionPriority priority, TArgs&&... constructor_args
) {
	return Transition<ToScene>(
		scene_.key_, Hash(scene_key), std::move(transition), priority,
		std::forward<TArgs>(constructor_args)...
	);
}

} // namespace ptgn