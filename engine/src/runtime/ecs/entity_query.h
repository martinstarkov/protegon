#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "runtime/ecs/entity.h"

namespace ptgn {

class Scene;

struct EntityQueryContext {
	Scene& scene;
	Entity owner{};
	Entity target{};
};

using EntityQueryCallback = bool (*)(const EntityQueryContext&);

struct RegisteredEntityQuery {
	std::string key{};
	std::string label{};
	std::string group{};
	std::string description{};
	EntityQueryCallback evaluate{ nullptr };
};

class EntityQueryRegistry {
public:
	static void Register(RegisteredEntityQuery query);

	[[nodiscard]] static const RegisteredEntityQuery* Find(std::string_view key);

	[[nodiscard]] static const std::vector<RegisteredEntityQuery>& Queries();
};

template <auto Function>
class AutoEntityQueryRegistration {
public:
	AutoEntityQueryRegistration(
		std::string key, std::string label, std::string group, std::string description
	) {
		EntityQueryRegistry::Register(RegisteredEntityQuery{
			.key = std::move(key),
			.label = std::move(label),
			.group = std::move(group),
			.description = std::move(description),
			.evaluate = Function,
		});
	}
};

#define PTGN_ENTITY_QUERY_CONCAT_IMPL(a, b) a##b
#define PTGN_ENTITY_QUERY_CONCAT(a, b) PTGN_ENTITY_QUERY_CONCAT_IMPL(a, b)
#define PTGN_REGISTER_ENTITY_QUERY(Key, Function, Label, Group, Description)                  \
	[[maybe_unused]] const ptgn::AutoEntityQueryRegistration<Function>                      \
		PTGN_ENTITY_QUERY_CONCAT(kEntityQueryRegistration_, __COUNTER__){                      \
			Key, Label, Group, Description                                                       \
		}

} // namespace ptgn
