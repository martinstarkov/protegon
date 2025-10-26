#include "core/ecs/entity.h"

#include <memory>
#include <utility>

#include "core/app/game.h"
#include "core/app/manager.h"
#include "core/ecs/component_registry.h"
#include "core/ecs/components/uuid.h"
#include "core/ecs/entity_hierarchy.h"
#include "core/util/type_info.h"
#include "debug/runtime/assert.h"
#include "ecs/ecs.h"
#include "renderer/render_target.h"
#include "serialization/json/fwd.h"
#include "serialization/json/json_archiver.h"
#include "world/scene/camera.h"
#include "world/scene/scene.h"
#include "world/scene/scene_key.h"
#include "world/scene/scene_manager.h"

namespace ptgn {

Entity::Entity(Scene& scene) : Entity{ scene.CreateEntity() } {}

ecs::impl::Index Entity::GetId() const {
	return BaseEntity::GetId();
}

Entity::Entity(const BaseEntity& entity) : BaseEntity{ entity } {}

void Entity::Clear() const {
	BaseEntity::Clear();
}

bool Entity::IsAlive() const {
	return BaseEntity::IsAlive();
}

Entity& Entity::Destroy(bool orphan_children) {
	if (*this == Entity{}) {
		return *this;
	}

	if (HasChildren(*this)) {
		auto children{ GetChildren(*this) };
		if (orphan_children) {
			for (Entity child : children) {
				impl::RemoveParentImpl(child);
			}
		} else {
			for (Entity child : children) {
				child.Destroy();
			}
		}
	}

	BaseEntity::Destroy();
	return *this;
}

Manager& Entity::GetManager() {
	return static_cast<Manager&>(BaseEntity::GetManager());
}

const Manager& Entity::GetManager() const {
	return static_cast<const Manager&>(BaseEntity::GetManager());
}

const Scene& Entity::GetScene() const {
	PTGN_ASSERT(Has<impl::SceneKey>());
	const auto& scene_key{ Get<impl::SceneKey>() };
	PTGN_ASSERT(game.scene.Has(scene_key));
	return game.scene.Get(scene_key);
}

Scene& Entity::GetScene() {
	return const_cast<Scene&>(std::as_const(*this).GetScene());
}

static RenderTarget GetParentRenderTarget(const Entity& root, const Entity& entity) {
	// @return Root or the entities render target or any of its parents' render targets (whichever
	// is first in the hierarchy).
	if (auto rt{ entity.TryGet<RenderTarget>() }) {
		return *rt;
	}
	if (HasParent(entity)) {
		Entity parent{ GetParent(entity) };
		return GetParentRenderTarget(root, parent);
	}
	return root;
}

const Camera& Entity::GetCamera() const {
	if (const auto camera{ GetNonPrimaryCamera() }) {
		return *camera;
	}
	if (const auto rt{ TryGet<RenderTarget>() }) {
		return rt->GetCamera();
	}
	if (RenderTarget rt{ GetParentRenderTarget(*this, *this) }; rt != *this) {
		PTGN_ASSERT(rt);
		return rt.GetCamera();
	}
	return GetScene().camera;
}

const Camera* Entity::GetNonPrimaryCamera() const {
	if (const auto camera{ TryGet<Camera>() }; camera && *camera) {
		return camera;
	}
	return nullptr;
}

Camera& Entity::GetCamera() {
	return const_cast<Camera&>(std::as_const(*this).GetCamera());
}

bool Entity::IsIdenticalTo(const Entity& e) const {
	return BaseEntity::IsIdenticalTo(e);
}

UUID Entity::GetUUID() const {
	PTGN_ASSERT(Has<UUID>(), "Every entity must have a UUID");
	return Get<UUID>();
}

std::size_t Entity::GetHash() const {
	return std::hash<BaseEntity>()(*this);
}

bool Entity::WasCreatedBefore(const Entity& other) const {
	PTGN_ASSERT(other != *this, "Cannot check if an entity was created before itself");
	auto version{ BaseEntity::GetVersion() };
	auto other_version{ other.BaseEntity::GetVersion() };
	if (version != other_version) {
		return version < other_version;
	}
	return BaseEntity::GetId() < other.BaseEntity::GetId();
}

void Entity::Invalidate() {
	*this = {};
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
	// TODO: Consider being able to fetch a manager using either a JSON key or the current scene.
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