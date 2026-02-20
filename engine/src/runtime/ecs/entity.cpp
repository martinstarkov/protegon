#include "runtime/ecs/entity.h"

#include <memory>
#include <utility>

#include "core/assert.h"
#include "core/util/type_info.h"
#include "ecs/ecs.h"
#include "runtime/ecs/component_registry.h"
#include "runtime/ecs/components/uuid.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/manager.h"
#include "runtime/scene/scene.h"
#include "serialization/json/archiver.h"
#include "serialization/json/fwd.h"

namespace ptgn {

Entity::Entity(Scene& scene) : Entity{ scene.CreateEntity() } {}

void Entity::Clear() const {
	entity_.Clear();
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

	entity_.Destroy();
	return *this;
}

Manager& Entity::GetManager() {
	return static_cast<Manager&>(entity_.GetManager());
}

const Manager& Entity::GetManager() const {
	return static_cast<const Manager&>(entity_.GetManager());
}

const Scene& Entity::GetScene() const {
	return *scene_;
}

Scene& Entity::GetScene() {
	return const_cast<Scene&>(std::as_const(*this).GetScene());
}

bool Entity::HasScene() const {
	return scene_ != nullptr;
}

bool Entity::IsIdenticalTo(const Entity& e) const {
	return entity_.IsIdenticalTo(e.entity_);
}

UUID Entity::GetUUID() const {
	PTGN_ASSERT(Has<UUID>(), "Every entity must have a UUID");
	return Get<UUID>();
}

std::size_t Entity::GetHash() const {
	return std::hash<ecs::impl::EntityHandle<JsonArchiver>>()(entity_);
}

bool Entity::WasCreatedBefore(const Entity& other) const {
	PTGN_ASSERT(other != *this, "Cannot check if an entity was created before itself");
	auto version{ entity_.GetVersion() };
	if (auto other_version{ other.entity_.GetVersion() }; version != other_version) {
		return version < other_version;
	}
	return entity_.GetId() < other.entity_.GetId();
}

void Entity::Invalidate() {
	*this = {};
}

void Entity::SerializeAllImpl(json& j) const {
	JsonArchiver archiver;

	const auto& pools{ GetManager().pools_ };

	for (const auto& pool : pools) {
		if (!pool) {
			continue;
		}
		pool->Serialize(archiver, entity_.GetId());
	}

	j = archiver.j;
}

void Entity::DeserializeAllImpl(const json& j) {
	JsonArchiver archiver;
	archiver.j = j;

	impl::ComponentRegistry::AddTypes(GetManager());

	auto& manager{ GetManager() };

	for (auto& pool : manager.pools_) {
		if (!pool) {
			continue;
		}
		pool->Deserialize(archiver, manager, entity_.GetId());
	}
}

void to_json(json& j, const Entity& entity) {
	j = json{};

	if (!entity) {
		return;
	}

	constexpr auto uuid_name{ type_name_without_namespaces<UUID>() };

	j[uuid_name] = entity.GetUUID();

	// TODO: Fix scene key serialization.
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

	// TODO: Fix scene key serialization.
}

std::size_t Hash(const Entity& entity) {
	return std::hash<Entity>()(entity);
}

} // namespace ptgn