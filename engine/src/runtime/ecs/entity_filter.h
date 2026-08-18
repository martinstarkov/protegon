#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "runtime/ecs/entity.h"
#include "runtime/ecs/uuid.h"
#include "serialization/serialize.h"

namespace ptgn {

class Scene;

enum class EntityFilterType : std::uint8_t {
	Any,
	Entity,
	Components,
	Group,
	Query,
};
PTGN_REFLECT_ENUM(EntityFilterType);

struct EntityReference {
	std::optional<UUID> uuid;
	std::string tag;

	PTGN_REFLECT(EntityReference, uuid, tag)
};

struct ComponentQueryCondition {
	std::string component;
	bool required{ true };

	PTGN_REFLECT(ComponentQueryCondition, component, required)
};

struct ComponentQueryGroup {
	std::vector<ComponentQueryCondition> conditions;

	PTGN_REFLECT(ComponentQueryGroup, conditions)
};

struct ComponentEntityQuery {
	std::vector<ComponentQueryGroup> groups;

	PTGN_REFLECT(ComponentEntityQuery, groups)
};

struct GroupEntityQuery {
	std::string group;

	PTGN_REFLECT(GroupEntityQuery, group)
};

struct RegisteredEntityQueryReference {
	std::string key;

	PTGN_REFLECT(RegisteredEntityQueryReference, key)
};

struct EntityFilter {
	EntityFilterType type{ EntityFilterType::Any };
	EntityReference entity;
	ComponentEntityQuery components;
	GroupEntityQuery group;
	RegisteredEntityQueryReference query;

	PTGN_REFLECT(EntityFilter, type, entity, components, group, query)
};

void SetEntityReference(EntityReference& reference, Entity entity);

[[nodiscard]] Entity ResolveEntityReference(Scene& scene, const EntityReference& reference);

[[nodiscard]] bool MatchesComponentQuery(Entity entity, const ComponentEntityQuery& query);

[[nodiscard]] bool Matches(
	const EntityFilter& filter, Scene& scene, Entity owner, Entity target
);

[[nodiscard]] std::vector<Entity> ResolveEntityFilter(
	const EntityFilter& filter, Scene& scene, Entity owner
);

} // namespace ptgn
