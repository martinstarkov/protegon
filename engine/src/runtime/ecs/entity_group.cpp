#include "runtime/ecs/entity_group.h"

#include <algorithm>
#include <memory>
#include <ranges>
#include <utility>
#include <vector>

namespace ptgn {

namespace {

std::vector<RegisteredEntityGroup>& Storage() {
	static std::vector<RegisteredEntityGroup> groups;
	return groups;
}

RegisteredEntityGroup* FindMutable(std::string_view key) {
	auto& groups{ Storage() };
	const auto it{ std::ranges::find(groups, key, &RegisteredEntityGroup::key) };
	return it == groups.end() ? nullptr : std::addressof(*it);
}

} // namespace

void EntityGroupRegistry::Register(std::string key, std::string label) {
	if (key.empty()) {
		return;
	}

	auto& groups{ Storage() };
	const auto it{ std::ranges::find(groups, key, &RegisteredEntityGroup::key) };

	if (it != groups.end()) {
		it->label = std::move(label);
		return;
	}

	groups.emplace_back(RegisteredEntityGroup{
		.key = std::move(key),
		.label = std::move(label),
	});
}

void EntityGroupRegistry::Add(std::string_view key, Entity entity) {
	if (!entity) {
		return;
	}

	auto* group{ FindMutable(key) };
	if (!group) {
		return;
	}

	const UUID uuid{ entity.Get<UUID>() };
	if (!std::ranges::contains(group->members, uuid)) {
		group->members.emplace_back(uuid);
	}
}

void EntityGroupRegistry::Remove(std::string_view key, Entity entity) {
	if (!entity) {
		return;
	}

	auto* group{ FindMutable(key) };
	if (!group) {
		return;
	}

	std::erase(group->members, entity.Get<UUID>());
}

bool EntityGroupRegistry::Contains(std::string_view key, Entity entity) {
	const auto* group{ Find(key) };
	return group && entity && std::ranges::contains(group->members, entity.Get<UUID>());
}

const RegisteredEntityGroup* EntityGroupRegistry::Find(std::string_view key) {
	const auto& groups{ Storage() };
	const auto it{ std::ranges::find(groups, key, &RegisteredEntityGroup::key) };
	return it == groups.end() ? nullptr : std::addressof(*it);
}

const std::vector<RegisteredEntityGroup>& EntityGroupRegistry::Groups() {
	return Storage();
}

} // namespace ptgn
