#include "core/entity.h"

#include <memory>
#include <utility>
#include <vector>

#include "common/assert.h"
#include "common/type_info.h"
#include "components/uuid.h"
#include "core/entity_hierarchy.h"
#include "core/game.h"
#include "core/manager.h"
#include "ecs/ecs.h"
#include "renderer/render_target.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "scene/scene_key.h"
#include "scene/scene_manager.h"
#include "serialization/component_registry.h"
#include "serialization/fwd.h"
#include "serialization/json_archiver.h"

namespace ptgn {

Entity::Entity(Scene& scene) : Entity{ scene.CreateEntity() } {}

ecs::impl::Index Entity::GetId() const {
	return Parent::GetId();
}

Entity::Entity(const Parent& entity) : Parent{ entity } {}

void Entity::Clear() const {
	Parent::Clear();
}

bool Entity::IsAlive() const {
	return Parent::IsAlive();
}

Entity& Entity::Destroy(bool orphan_children) {
	if (*this == Entity{}) {
		return *this;
	}

	if (!orphan_children && HasChildren(*this)) {
		auto children{ GetChildren(*this) };
		for (Entity child : children) {
			child.Destroy();
		}
	}

	Parent::Destroy();
	return *this;
}

Manager& Entity::GetManager() {
	return static_cast<Manager&>(Parent::GetManager());
}

const Manager& Entity::GetManager() const {
	return static_cast<const Manager&>(Parent::GetManager());
}

const Scene& Entity::GetScene() const {
	PTGN_ASSERT(Has<impl::SceneKey>());
	const auto& scene_key{ Get<impl::SceneKey>() };
	PTGN_ASSERT(game.scene.HasScene(scene_key));
	return game.scene.Get(scene_key);
}

Scene& Entity::GetScene() {
	return const_cast<Scene&>(std::as_const(*this).GetScene());
}

RenderTarget GetParentRenderTarget(const Entity& entity) {
	if (entity.Has<RenderTarget>()) {
		return entity.Get<RenderTarget>();
	}
	if (HasParent(entity)) {
		return GetParentRenderTarget(GetParent(entity));
	}
	return entity;
}

const Camera& Entity::GetCamera() const {
	Camera* camera{ nullptr };
	if (Has<Camera>()) {
		camera = &Get<Camera>();
	}
	if (camera && *camera) {
		return *camera;
	}
	if (Has<RenderTarget>()) {
		if (auto& rt{ Get<RenderTarget>() }; rt.Has<Camera>()) {
			camera = &rt.Get<Camera>();
		}
	}
	if (camera && *camera) {
		return *camera;
	}
	if (auto rt{ GetParentRenderTarget(*this) }; rt != *this && rt && rt.Has<Camera>()) {
		camera = &rt.Get<Camera>();
	}
	if (camera && *camera) {
		return *camera;
	}
	return GetScene().camera.primary;
}

Camera& Entity::GetCamera() {
	return const_cast<Camera&>(std::as_const(*this).GetCamera());
}

bool Entity::IsIdenticalTo(const Entity& e) const {
	return Parent::IsIdenticalTo(e);
}

UUID Entity::GetUUID() const {
	PTGN_ASSERT(Has<UUID>(), "Every entity must have a UUID");
	return Get<UUID>();
}

std::size_t Entity::GetHash() const {
	return std::hash<Parent>()(*this);
}

bool Entity::WasCreatedBefore(const Entity& other) const {
	PTGN_ASSERT(other != *this, "Cannot check if an entity was created before itself");
	auto version{ Parent::GetVersion() };
	auto other_version{ other.Parent::GetVersion() };
	if (version != other_version) {
		return version < other_version;
	}
	return Parent::GetId() < other.Parent::GetId();
}

void Entity::SerializeAllImpl(json& j) const {
	JSONArchiver archiver;

	PTGN_ASSERT(manager_ != nullptr);

	const auto& pools{ GetManager().pools_ };

	for (const auto& pool : pools) {
		if (!pool) {
			continue;
		}
		pool->Serialize(archiver, entity_);
	}

	j = archiver.j;
}

void Entity::DeserializeAllImpl(const json& j) {
	JSONArchiver archiver;
	archiver.j = j;

	impl::ComponentRegistry::AddTypes(GetManager());

	auto& manager{ GetManager() };

	for (auto& pool : manager.pools_) {
		if (!pool) {
			continue;
		}
		pool->Deserialize(archiver, manager, entity_);
	}
}

void Scripts::Update(Scene& scene, float dt) {
	Invoke<&impl::IScript::OnUpdate>(scene, dt);

	scene.Refresh();
}

std::vector<Entity> Scripts::GetEntities(Scene& scene) {
	return scene.EntitiesWith<Scripts>().GetVector();
}

void to_json(json& j, const Entity& entity) {
	j = json{};

	if (!entity) {
		return;
	}

	constexpr auto uuid_name{ type_name_without_namespaces<UUID>() };

	j[uuid_name] = entity.GetUUID();

	if (entity.Has<impl::SceneKey>()) {
		constexpr auto scene_key_name{ type_name_without_namespaces<impl::SceneKey>() };
		j[scene_key_name] = entity.Get<impl::SceneKey>();
	}
}

void from_json(const json& j, Entity& entity) {
	PTGN_ASSERT(entity, "Cannot read JSON into null entity");

	constexpr auto uuid_name{ type_name_without_namespaces<UUID>() };

	PTGN_ASSERT(
		j.contains(uuid_name), "Cannot create entity from JSON which does not contain a UUID"
	);

	UUID uuid;

	j[uuid_name].get_to(uuid);

	const auto& manager{ entity.GetManager() };

	auto found_entity{ manager.GetEntityByUUID(uuid) };

	PTGN_ASSERT(!found_entity || (found_entity && found_entity == entity));

	PTGN_ASSERT(entity, "Failed to find entity with UUID: ", uuid);

	constexpr auto scene_key_name{ type_name_without_namespaces<impl::SceneKey>() };

	if (j.contains(scene_key_name)) {
		impl::SceneKey scene_key;
		j[scene_key_name].get_to(scene_key);
		entity.Add<impl::SceneKey>(scene_key);
	}
}

} // namespace ptgn