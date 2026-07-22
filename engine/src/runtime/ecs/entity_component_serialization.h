#pragma once

#include <string>
#include <string_view>

#include "runtime/ecs/component_registry.h"
#include "runtime/ecs/entity.h"
#include "serialization/json/json.h"

namespace ptgn {

inline json SerializeEntityComponents(Entity entity) {
	json output;

	for (const auto& component : ComponentRegistry::Components()) {
		if (!component.serialize || !component.has(entity)) {
			continue;
		}

		component.serialize(output[std::string{ component.name }], entity);
	}

	return output;
}

inline void DeserializeEntityComponents(const json& input, Entity entity) {
	for (const auto& [name, component_json] : input.items()) {
		const auto* component{ ComponentRegistry::Find(std::string_view{ name }) };

		if (!component || !component->deserialize) {
			continue;
		}

		component->deserialize(component_json, entity);
	}
}

} // namespace ptgn
