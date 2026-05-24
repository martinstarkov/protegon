#pragma once

#include <ecs/ecs.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/event/event.h"
#include "core/graphics/color.h"
#include "core/util/concepts.h"
#include "renderer/pipeline/camera.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/manager.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/render_target.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_common.h"
#include "runtime/scene/scene_transition.h"
#include "runtime/scene/scene_view.h"
#include "serialization/json/archiver.h"
#include "serialization/json/json.h"

namespace ptgn {

class Application;
class Scene;
class EventHandler;
class SceneContext;
class LocalSceneManager;
class DrawContext;
class CollisionHandler;

namespace impl {

class Renderer;
struct RenderCamera;

enum class SceneState {
	Active,
	TransitionIn,
	TransitionOut
};

struct SceneData {
	std::string tag;
	std::size_t tag_hash{ 0 };
	impl::SceneState state{ impl::SceneState::Active };
	std::unique_ptr<SceneTransition> transition;
};

template <SceneType TScene>
void InitScene(TScene& scene, Application& app, SceneData&& scene_data);

} // namespace impl

template <typename TComponent>
struct SceneHook {
	Scene& scene;
	ecs::Hook<void, ecs::impl::BaseEntity<JsonArchiver>>& hook;

	template <auto Member>
	void Connect();
};

class Scene {
public:
	Scene() = default;

	Scene(Scene&&) noexcept;
	Scene& operator=(Scene&&) noexcept;

	Scene(const Scene&)			   = delete;
	Scene& operator=(const Scene&) = delete;

	virtual ~Scene();

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
	virtual void OnEvent(Event) {
		/* user implementation */
	}

	/// @brief Sets the background color of the scene. The default background color is transparent.
	void SetBackgroundColor(Color background_color);

	/// @return The background color of the scene.
	Color GetBackgroundColor() const;

	/// @return Null entity if no entity with the given uuid exists in the manager.
	Entity GetEntityByUUID(std::uint64_t uuid) const;

	/// @return Null entity if no entity with the given tag exists in the scene.
	/// If multiple entities have the same tag, returns the first one found.
	Entity GetEntityByTag(std::string_view tag) const;

	/// @brief Make sure to call Refresh() after this function.

	/// @brief Creates an entity with a specified tag and UUID, or a default tag and a random UUID
	/// if unspecified.
	/// Make sure to call Refresh() after this function.
	Entity CreateEntity(
		std::optional<std::string_view> tag = std::nullopt,
		std::optional<std::uint64_t> uuid	= std::nullopt
	);

	/// @brief Creates an entity from a json object.
	/// If the json does not have a valid tag component, the default tag will be assigned.
	/// If the json does not have a valid UUID component, a random UUID will be assigned.
	/// Make sure to call Refresh() after this function.
	Entity CreateEntity(const json& j);

	/// @brief Copies all of the from entity's specified components into a new entity with a
	/// specified tag and UUID, or a default tag and a random UUID if unspecified.
	/// @brief Make sure to call Refresh() after this function.
	template <typename... Ts>
	Entity CopyEntity(
		Entity from, std::optional<std::string_view> tag = std::nullopt,
		std::optional<std::uint64_t> uuid = std::nullopt
	) {
		auto entity{ manager_.CopyEntity<Ts...>(from) };
		AddMandatoryComponents(entity, tag, uuid);
		return entity;
	}

	template <typename... Ts>
	void CopyEntity(const Entity& from, Entity& to) {
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

	std::size_t GetTagHash() const;

	std::string GetTag() const;

	[[nodiscard]] bool IsTransitioning() const;

private:
	friend class impl::SceneManager;
	friend class EventHandler;
	friend class Application;
	friend class FrameContext;
	friend class SceneInput;
	friend class LocalEventHandler;
	template <typename TComponent>
	friend struct SceneHook;

	template <SceneType TScene>
	friend void impl::InitScene(TScene& scene, Application& app, impl::SceneData&& scene_data);

	void Init(Application& app, impl::SceneData&& scene_data);

	template <typename TScene, auto Member>
	void HookThunk(ecs::impl::BaseEntity<JsonArchiver> handle) {
		(static_cast<TScene*>(this)->*Member)(Entity{ handle, this });
	}

	/// @brief Called by scene manager when a new scene is loaded and entered.
	void InternalEnter();
	void InternalExit();
	void InternalOnEvent(Event event);
	void InternalOnEvent();
	void InternalPreUpdate();

	void InternalUpdate();
	void InternalDraw(DrawContext& draw_context);
	[[nodiscard]] bool IsAwaitingTransitionDelay() const;

	std::unique_ptr<SceneContext> ctx_;

	Manager manager_;
	RenderTarget render_target_;

	impl::SceneData data_;
};

namespace impl {

template <typename T>
struct MemberPointerClass;

template <typename C, typename R, typename... Args>
struct MemberPointerClass<R (C::*)(Args...)> {
	using type = C;
};

template <typename C, typename R, typename... Args>
struct MemberPointerClass<R (C::*)(Args...) const> {
	using type = C;
};

template <SceneType TScene>
void InitScene(TScene& scene, Application& app, SceneData&& scene_data) {
	scene.Init(app, std::move(scene_data));
}

template <InvocableR<void, Scene&, const RenderCamera&, EntityFilterFunc> TFunc>
void ForDrawableSceneEntities(
	Scene& scene, const std::optional<Camera>& primary_world_camera, TFunc per_camera_draw_func
) {
	if (primary_world_camera.has_value()) {
		// If a primary world camera is set, we draw all entities in a single pass using that
		// camera.
		RenderCamera render_camera{ *primary_world_camera };

		auto filter = [](auto) {
			// TODO: Add frustum culling.
			// if (!std::ranges::contains(frustum_objects, entity)) {
			//	continue;
			//}

			return false;
		};

		per_camera_draw_func(scene, render_camera, filter);

	} else {
		std::vector<Entity> camera_entities;

		for (auto [camera_entity, _data] : scene.EntitiesWith<CameraData>()) {
			camera_entities.emplace_back(camera_entity);
		}

		SortByDepth(camera_entities, false);

		for (const auto& camera_entity : camera_entities) {
			SceneCamera scene_camera{ camera_entity };
			RenderCamera render_camera{ scene_camera };

			auto filter = [&scene_camera](auto entity) {
				// TODO: Add frustum culling.
				// if (!std::ranges::contains(frustum_objects, entity)) {
				//	continue;
				//}

				return !scene_camera.IsVisible(entity);
			};

			per_camera_draw_func(scene, render_camera, filter);
		}
	}
}

} // namespace impl

template <typename TComponent>
template <auto Member>
void SceneHook<TComponent>::Connect() {
	using TScene = typename impl::MemberPointerClass<decltype(Member)>::type;

	hook.template Connect<Scene, &Scene::template HookThunk<TScene, Member>>(&scene);
}

} // namespace ptgn