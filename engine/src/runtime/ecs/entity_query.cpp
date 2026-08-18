#include "runtime/ecs/entity_query.h"

#include <algorithm>
#include <memory>
#include <ranges>
#include <utility>

namespace ptgn {

namespace {

std::vector<RegisteredEntityQuery>& Storage() {
	static std::vector<RegisteredEntityQuery> queries;
	return queries;
}

} // namespace

void EntityQueryRegistry::Register(RegisteredEntityQuery query) {
	if (query.key.empty() || !query.evaluate) {
		return;
	}

	auto& queries{ Storage() };
	const auto it{ std::ranges::find(queries, query.key, &RegisteredEntityQuery::key) };

	if (it != queries.end()) {
		*it = std::move(query);
		return;
	}

	queries.emplace_back(std::move(query));
}

const RegisteredEntityQuery* EntityQueryRegistry::Find(std::string_view key) {
	const auto& queries{ Storage() };
	const auto it{ std::ranges::find(queries, key, &RegisteredEntityQuery::key) };
	return it == queries.end() ? nullptr : std::addressof(*it);
}

const std::vector<RegisteredEntityQuery>& EntityQueryRegistry::Queries() {
	return Storage();
}

} // namespace ptgn
