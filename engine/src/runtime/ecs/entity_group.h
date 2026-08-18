#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "runtime/ecs/entity.h"
#include "runtime/ecs/uuid.h"

namespace ptgn {

struct RegisteredEntityGroup {
	std::string key;
	std::string label;
	std::vector<UUID> members;
};

class EntityGroupRegistry {
public:
	static void Register(std::string key, std::string label);

	static void Add(std::string_view key, Entity entity);
	static void Remove(std::string_view key, Entity entity);

	[[nodiscard]] static bool Contains(std::string_view key, Entity entity);
	[[nodiscard]] static const RegisteredEntityGroup* Find(std::string_view key);
	[[nodiscard]] static const std::vector<RegisteredEntityGroup>& Groups();
};

} // namespace ptgn
