#include "runtime/ecs/entity_filter.h"

#include <algorithm>
#include <ranges>
#include <string_view>
#include <vector>

#include "runtime/ecs/component_registry.h"
#include "runtime/ecs/entity_group.h"
#include "runtime/ecs/entity_query.h"
#include "runtime/ecs/tag.h"
#include "runtime/scene/scene.h"

namespace ptgn {

namespace {

bool MatchesComponentCondition(Entity entity, const ComponentQueryCondition& condition) {
	if (condition.component.empty()) {
		return false;
	}

	const auto* component{ ComponentRegistry::Find(std::string_view{ condition.component }) };
	if (!component) {
		return false;
	}

	const bool has_component{ component->Has(entity) };
	return condition.required ? has_component : !has_component;
}

bool MatchesComponentGroup(Entity entity, const ComponentQueryGroup& group) {
	if (group.conditions.empty()) {
		return false;
	}

	return std::ranges::all_of(group.conditions, [entity](const auto& condition) {
		return MatchesComponentCondition(entity, condition);
	});
}

} // namespace

void SetEntityReference(EntityReference& reference, Entity entity) {
	if (!entity) {
		reference = {};
		return;
	}

	reference.uuid = entity.Get<UUID>();
	if (const auto tag{ entity.TryGet<Tag>() }) {
		reference.tag = tag->value;
	} else {
		reference.tag.clear();
	}
}

Entity ResolveEntityReference(Scene& scene, const EntityReference& reference) {
	if (!reference.uuid.has_value()) {
		return {};
	}

	for (Entity entity : scene.Entities()) {
		if (entity.Get<UUID>() == reference.uuid.value()) {
			return entity;
		}
	}

	return {};
}

bool MatchesComponentQuery(Entity entity, const ComponentEntityQuery& query) {
	if (!entity || query.groups.empty()) {
		return false;
	}

	return std::ranges::any_of(query.groups, [entity](const auto& group) {
		return MatchesComponentGroup(entity, group);
	});
}

bool Matches(const EntityFilter& filter, Scene& scene, Entity owner, Entity target) {
	if (!target) {
		return false;
	}

	switch (filter.type) {
		case EntityFilterType::Any:
			return true;
		case EntityFilterType::Entity: {
			const Entity resolved{ ResolveEntityReference(scene, filter.entity) };
			return resolved && resolved == target;
		}
		case EntityFilterType::Components:
			return MatchesComponentQuery(target, filter.components);
		case EntityFilterType::Group: {
			auto* membership{ target.TryGet<Group>() };
			if (!membership) {
				return false;
			}

			return std::ranges::any_of(
				filter.group.groups,
				[&membership](const std::string& group) {
					return !group.empty() &&
						std::ranges::contains(membership->groups, group);
				}
			);
		}
		case EntityFilterType::Query: {
			const auto* query{ EntityQueryRegistry::Find(filter.query.key) };
			return query && query->evaluate && query->evaluate(EntityQueryContext{
				.scene = scene,
				.owner = owner,
				.target = target,
			});
		}
	}

	return false;
}

std::vector<Entity> ResolveEntityFilter(const EntityFilter& filter, Scene& scene, Entity owner) {
	std::vector<Entity> matches;

	for (Entity entity : scene.Entities()) {
		if (Matches(filter, scene, owner, entity)) {
			matches.emplace_back(entity);
		}
	}

	return matches;
}

} // namespace ptgn
