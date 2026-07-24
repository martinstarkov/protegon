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

/// @brief Whether a registered component is serialized as an entry in the entity's tags array.
[[nodiscard]] inline bool IsSerializedTagComponent(const RegisteredComponent& component) {
	return component.is_empty && !IsSceneMetadataComponent(component);
}

} // namespace impl

/// @brief Serializes empty marker components as registered type names.
///
/// Empty components do not require JSON reflection or custom serialization.
[[nodiscard]] inline json SerializeEntityTags(Entity entity) {
	json output = json::array();

	for (const auto& component : ComponentRegistry::Components()) {
		if (!impl::IsSerializedTagComponent(component) ||
			!component.has ||
			!component.has(entity)) {
			continue;
		}

		output.push_back(std::string{ component.name });
	}

	return output;
}

/// @brief Serializes ordinary entity components.
///
/// Empty marker components are serialized separately by SerializeEntityTags.
/// UUID, Tag, Parent, and Children are serialized by the scene as entity metadata.
[[nodiscard]] inline json SerializeEntityComponents(Entity entity) {
	json output = json::object();

	for (const auto& component : ComponentRegistry::Components()) {
		if (impl::IsSceneMetadataComponent(component) ||
			component.is_empty ||
			!component.serializable ||
			!component.deserializable ||
			!component.serialize ||
			!component.deserialize ||
			!component.has ||
			!component.has(entity)) {
			continue;
		}

		json value = json::object();
		component.serialize(value, entity);

		// Skips empty containers.
		// Currently not desired since "Scripts": [] is a convenient way to have the scripts component stay on an entity.
		// if ((value.is_array() || value.is_object()) && value.empty()) {
		// 	continue;
		// }

		output[std::string{ component.name }] = std::move(value);

	}

	return output;
}

/// @brief Adds registered empty components listed in an entity's tags array.
///
/// Unknown tags and registered tags that cannot be default-constructed are ignored.
inline void DeserializeEntityTags(const json& input, Entity entity) {
	PTGN_ASSERT(input.is_array(), "Serialized entity tags must be a JSON array");
	PTGN_ASSERT(entity, "Cannot deserialize tags into a null entity");

	for (const auto& tag_json : input) {
		if (!tag_json.is_string()) {
			continue;
		}

		const auto& name{ tag_json.get_ref<const std::string&>() };
		const auto* component{ ComponentRegistry::Find(std::string_view{ name }) };

		if (!component ||
			!impl::IsSerializedTagComponent(*component) ||
			!component->add_default ||
			!component->has) {
			continue;
		}

		if (!component->has(entity)) {
			component->add_default(entity);
		}
	}
}

/// @brief Deserializes ordinary entity components into an already-created entity.
///
/// Empty marker components must be stored in the entity's "tags" array.
/// UUID, Tag, Parent, and Children are ignored because the scene loader owns them.
inline void DeserializeEntityComponents(const json& input, Entity entity) {
	PTGN_ASSERT(input.is_object(), "Serialized entity components must be a JSON object");
	PTGN_ASSERT(entity, "Cannot deserialize components into a null entity");

	for (const auto& [name, component_json] : input.items()) {
		const auto* component{ ComponentRegistry::Find(std::string_view{ name }) };

		if (!component ||
			impl::IsSceneMetadataComponent(*component) ||
			component->is_empty ||
			!component->deserializable ||
			!component->deserialize) {
			continue;
		}

		component->deserialize(component_json, entity);
	}
}

} // namespace ptgn