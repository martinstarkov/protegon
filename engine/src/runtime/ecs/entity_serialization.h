#pragma once

#include <string>
#include <string_view>
#include <utility>

#include "core/assert.h"
#include "core/util/hash.h"
#include "runtime/ecs/component_registry.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/relatives.h"
#include "runtime/ecs/tag.h"
#include "runtime/ecs/uuid.h"
#include "serialization/json/json.h"

namespace ptgn {

namespace impl {

[[nodiscard]] inline bool IsSceneMetadataComponent(const RegisteredComponent& component) {
	return component.type_id == Hash<UUID>() ||
		   component.type_id == Hash<Tag>() ||
		   component.type_id == Hash<Parent>() ||
		   component.type_id == Hash<Children>();
}

} // namespace impl

/// @brief Serializes ordinary entity components.
/// UUID, Tag, Parent, and Children are serialized by the scene as entity metadata.
[[nodiscard]] inline json SerializeEntityComponents(Entity entity) {
	json output = json::object();

	for (const auto& component : ComponentRegistry::Components()) {
		if (impl::IsSceneMetadataComponent(component) || !component.serializable ||
			!component.deserializable || !component.serialize || !component.deserialize ||
			!component.has(entity)) {
			continue;
		}

		json value;
		component.serialize(value, entity);
		output[std::string{ component.name }] = std::move(value);
	}

	return output;
}

/// @brief Deserializes ordinary entity components into an already-created entity.
/// UUID, Tag, Parent, and Children are intentionally ignored because the scene loader owns them.
inline void DeserializeEntityComponents(const json& input, Entity entity) {
	PTGN_ASSERT(input.is_object(), "Serialized entity components must be a JSON object");
	PTGN_ASSERT(entity, "Cannot deserialize components into a null entity");

	for (const auto& [name, component_json] : input.items()) {
		const auto* component{ ComponentRegistry::Find(std::string_view{ name }) };

		if (!component || impl::IsSceneMetadataComponent(*component) || !component->deserializable ||
			!component->deserialize) {
			continue;
		}

		component->deserialize(component_json, entity);
	}
}

} // namespace ptgn
