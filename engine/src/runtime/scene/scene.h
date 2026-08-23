#pragma once

#include <ecs/ecs.h>

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "core/event/event.h"
#include "core/graphics/color.h"
#include "runtime/asset/asset_key.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/manager.h"
#include "runtime/ecs/tag.h"
#include "runtime/ecs/uuid.h"
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
class LocalEventHandler;
class SceneContext;
class DrawContext;

/// @brief Dependency builder used before a scene is constructed and initialized.
/// Add assets that may be needed by entities created later at runtime and therefore
/// cannot be discovered from the serialized ECS content.
class AssetPreloadContext {
public:
	void Add(AssetKey key) {
		if (!key.value.empty() && !std::ranges::contains(dependencies_, key)) {
			dependencies_.emplace_back(std::move(key));
		}
	}

	template <typename T>
		requires std::derived_from<std::remove_cvref_t<T>, AssetKey>
	void Add(T key) {
		Add(AssetKey{ std::move(key.value) });
	}

	[[nodiscard]] const std::vector<AssetKey>& GetDependencies() const {
		return dependencies_;
	}

private:
	std::vector<AssetKey> dependencies_;
};

namespace impl {

class SceneFileAccess;

enum class SceneState {
	Active,
	TransitionIn,
	TransitionOut
};

struct SceneData {
	std::string tag{};
	std::size_t tag_hash{ 0 };
	impl::SceneState state{ impl::SceneState::Active };
	std::unique_ptr<SceneTransition> transition{};
	bool runtime{ true };
	bool first_scene{ false };
	std::string registered_type{};
	bool render_enabled{ true };
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

	Scene(const Scene&) = delete;
	Scene& operator=(const Scene&) = delete;

	virtual ~Scene();

	/// @brief Called on a temporary, uninitialized scene before asynchronous loading begins.
	/// Keep constructors and this method free of renderer/window access.
	virtual void OnPreload(AssetPreloadContext&) const {
		/* User implementation */
	}

	virtual void OnNew() {
		/* User implementation */
	}

	virtual void OnLoad() {
		/* User implementation */
	}

	virtual void OnEnter() {
		/* User implementation */
	}

	virtual void OnUpdate() {
		/* User implementation */
	}

	virtual void OnExit() {
		/* User implementation */
	}

	virtual void OnEvent(Event) {
		/* User implementation */
	}

	void SetBackgroundColor(Color background_color);
	Color GetBackgroundColor() const;

	Entity GetEntity(UUID uuid) const;
	Entity GetEntity(const Tag& tag) const;

	Entity CreateEntity(Tag tag = {}, UUID uuid = {});

	Entity CreatePrefab(std::string_view prefab_key);
	Entity CreatePrefab(const PrefabKey& prefab_key);

	template <typename... Ts>
	Entity CopyEntity(Entity from, Tag tag = {}, UUID uuid = {}) {
		auto entity{ manager_.CopyEntity<Ts...>(from.entity_) };
		entity.template Add<Tag>(std::move(tag));
		entity.template Add<UUID>(uuid);
		return Entity{ entity, this };
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

	/// @brief Adds a persistent preload-only dependency that serialized-content discovery cannot see.
	bool AddAssetDependency(AssetKey key);

	/// @brief Removes a persistent preload-only scene dependency. Automatically discovered references
	/// remain part of the scene until the serialized field referencing them is changed.
	bool RemoveAssetDependency(const AssetKey& key);

	/// @return True when the asset is either explicitly preloaded or referenced by serialized scene data.
	[[nodiscard]] bool HasAssetDependency(const AssetKey& key) const;

	/// @return True only for dependencies explicitly added through code or the editor context menu.
	[[nodiscard]] bool HasExplicitAssetDependency(const AssetKey& key) const;

	/// @return The effective dependency set used for scene residency and scene-file preloading.
	[[nodiscard]] const std::vector<AssetKey>& GetAssetDependencies() const;

	/// @return Explicit preload-only dependencies. CaptureScene combines these with serialized refs.
	[[nodiscard]] const std::vector<AssetKey>& GetExplicitAssetDependencies() const;

	/// @brief Rebuilds effective dependencies from the current serialized parameters/content plus
	/// explicit preload-only dependencies. Added assets are acquired and removed assets are released.
	/// @return True when the effective dependency set changed.
	bool SyncAssetDependenciesFromSerialization();

	/// @brief Reacquires the current effective dependency set for an already-live scene.
	void ReloadLoadedAssetDependencies();

	void Refresh();

	std::size_t GetEntityCount() const;

	RenderTarget GetRenderTarget() const;
	SceneCamera GetCamera() const;
	SceneCamera GetFixedCamera() const;

	[[nodiscard]] const SceneContext& ctx() const;
	[[nodiscard]] SceneContext& ctx();

	std::size_t GetTagHash() const;
	std::string GetTag() const;

	void SetRenderEnabled(bool enabled = true);

	[[nodiscard]] bool IsRenderEnabled() const;
	[[nodiscard]] bool IsRuntime() const;
	[[nodiscard]] bool IsTransitioning() const;
	[[nodiscard]] std::string_view GetRegisteredType() const;

	[[nodiscard]] json SerializeContent() const;

private:
	friend class impl::SceneManager;
	friend class impl::SceneFileAccess;
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
	void Init(Application& app, impl::SceneData&& scene_data, const json& serialized_content);

	void InitBase(Application& app, impl::SceneData&& scene_data);
	void CreateDefaultSceneEntities();
	void DeserializeContent(const json& serialized_content);

	template <typename TScene, auto Member>
	void HookThunk(ecs::impl::BaseEntity<JsonArchiver> handle) {
		(static_cast<TScene*>(this)->*Member)(Entity{ handle, this });
	}

	void InternalEnter();
	void InternalExit();
	void InternalOnEvent(Event event);
	void InternalOnEvent();
	void InternalPreUpdate();

	void InternalUpdate();
	void InternalRuntimeUpdate();
	void InternalMaintenanceUpdate();
	void InternalDraw(DrawContext& draw_context);
	void ClearRenderTargets();
	void DrawCameras(DrawContext& draw_context, const std::vector<Entity>& cameras);
	void DrawSceneTarget(DrawContext& draw_context) const;
	[[nodiscard]] bool IsAwaitingTransitionDelay() const;

	void SetAssetDependencies(
		std::vector<AssetKey> dependencies,
		std::vector<AssetKey> explicit_dependencies
	);
	void AdoptLoadedAssetDependencies(std::vector<AssetKey> dependencies);
	void ReleaseLoadedAssetDependencies() noexcept;

	std::unique_ptr<SceneContext> ctx_;
	Manager manager_;
	impl::SceneData data_;
	std::vector<AssetKey> asset_dependencies_;
	std::vector<AssetKey> explicit_asset_dependencies_;
	std::vector<AssetKey> retained_asset_dependencies_;
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

} // namespace impl

template <typename TComponent>
template <auto Member>
void SceneHook<TComponent>::Connect() {
	using TScene = typename impl::MemberPointerClass<decltype(Member)>::type;
	hook.template Connect<Scene, &Scene::template HookThunk<TScene, Member>>(&scene);
}

} // namespace ptgn
