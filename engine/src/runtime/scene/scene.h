#pragma once

#include <concepts>
#include <memory>

#include "core/event/dispatcher.h"
#include "ecs/ecs.h"
#include "renderer/primitives/color.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/manager.h"
#include "runtime/graphics/render_target_component.h"
#include "runtime/scene/scene_view.h"
#include "serialization/json/archiver.h"
#include "serialization/json/fwd.h"

namespace ptgn {

class Application;
class Scene;
class SceneContext;
class SceneEventHandler;

template <typename TComponent>
struct SceneHook {
	Scene& scene;
	ecs::Hook<void, ecs::impl::EntityHandle<JsonArchiver>>& hook;

	template <auto Member>
	void Connect();
};

class Scene {
public:
	Scene();
	virtual ~Scene();

	/// Called when the scene is added to active scenes.
	virtual void OnEnter() {
		/* user implementation */
	}

	/// Called once per frame for each active scene.
	virtual void OnUpdate() {
		/* user implementation */
	}

	/// Called when the scene is removed from active scenes.
	virtual void OnExit() {
		/* user implementation */
	}

	/// Called an event is emitted by the event handler.
	virtual void OnEvent(EventDispatcher) {
		/* user implementation */
	}

	// TODO: Fix.
	///// Call to simulate the scene being re-entered.
	// void ReEnter();

	/// @brief Sets the background color of the scene. The default background color is transparent.
	void SetBackgroundColor(Color background_color);

	/// @return The background color of the scene.
	Color GetBackgroundColor() const;

	/// @return {} if no entity with the given uuid exists in the manager.
	Entity GetEntityByUUID(UUID uuid) const;

	/// Make sure to call Refresh() after this function.
	Entity CreateEntity();

	/// Make sure to call Refresh() after this function.
	/// Creates an entity with a specific uuid.
	Entity CreateEntity(UUID uuid);

	/// Make sure to call Refresh() after this function.
	/// Creates an entity from a json object.
	Entity CreateEntity(const json& j);

	/// Make sure to call Refresh() after this function.
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
	friend class FrameContext;
	friend class SceneInput;
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

	std::unique_ptr<SceneContext> ctx_;
	Manager manager_;
	RenderTarget render_target_;
};

template <typename T>
concept SceneType = std::derived_from<T, Scene>;

template <typename TComponent>
template <auto Member>
void SceneHook<TComponent>::Connect() {
	hook.template Connect<Scene, &Scene::template HookThunk<Member>>(&scene);
}

} // namespace ptgn