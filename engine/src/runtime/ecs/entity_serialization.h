#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/util/hash.h"
#include "runtime/ecs/component_registry.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/relatives.h"
#include "runtime/ecs/tag.h"
#include "runtime/graphics/draw.h"
#include "runtime/ecs/uuid.h"
#include "serialization/json/json.h"
#include "serialization/serialize.h"

namespace ptgn {

using SerializedComponentMap = std::map<std::string, json>;
using SerializedEntityPath = std::vector<std::size_t>;

/// @brief Complete persistent representation of an entity subtree.
///
/// UUID is optional because scenes and editor snapshots preserve identity,
/// while prefab instances receive fresh UUIDs.
///
/// Parent is deliberately excluded. Child relationships inside the serialized
/// subtree are represented by children. A parent outside the subtree is
/// restoration context and is stored by the caller when needed.
struct SerializedEntity {
	std::optional<UUID> uuid;
	std::string tag{ "Entity" };

	/// Empty registered marker components, stored by registered type name.
	std::vector<std::string> tags;

	/// Non-empty persistent components, keyed by registered type name.
	SerializedComponentMap components;

	/// Complete serialized child hierarchy.
	std::vector<SerializedEntity> children;

	PTGN_REFLECT(
		SerializedEntity,
		uuid,
		tag,
		tags,
		components,
		children
	)
};

struct SerializeEntityOptions {
	/// Preserve the entity's UUID in the serialized result.
	/// Scenes and snapshots use true. Prefabs use false.
	bool include_uuid{ true };

	/// Recursively capture the entity's complete child hierarchy.
	bool include_children{ true };
};

namespace impl {

[[nodiscard]] inline bool IsEntityMetadataComponent(
	const RegisteredComponent& component
) {
	return component.type_id == Hash<UUID>() ||
		   component.type_id == Hash<Tag>() ||
		   component.type_id == Hash<Parent>() ||
		   component.type_id == Hash<Children>();
}

[[nodiscard]] inline bool IsSerializedTagComponent(
	const RegisteredComponent& component
) {
	return component.is_empty &&
		   !IsEntityMetadataComponent(component);
}

[[nodiscard]] inline std::vector<std::string>
SerializeEntityTagComponents(Entity entity) {
	std::vector<std::string> output;

	for (const auto& component : ComponentRegistry::Components()) {
		if (!IsSerializedTagComponent(component) ||
			!component.Has(entity)) {
			continue;
		}

		output.emplace_back(component.name);
	}

	return output;
}

[[nodiscard]] inline SerializedComponentMap
SerializeEntityValueComponents(Entity entity) {
	SerializedComponentMap output;

	for (const auto& component : ComponentRegistry::Components()) {
		if (IsEntityMetadataComponent(component) ||
			component.is_empty ||
			!component.serializable ||
			!component.deserializable ||
			!component.Has(entity)) {
			continue;
		}

		json value = json::object();

		if (!component.Serialize(value, entity)) {
			continue;
		}

		output.insert_or_assign(
			std::string{ component.name },
			std::move(value)
		);
	}

	return output;
}

inline void DeserializeEntityTagComponents(
	const std::vector<std::string>& tags,
	Entity entity
) {
	for (const auto& name : tags) {
		const auto* component{
			ComponentRegistry::Find(std::string_view{ name })
		};

		if (!component ||
			!IsSerializedTagComponent(*component) ||
			!component->default_constructible) {
			continue;
		}

		if (!component->Has(entity)) {
			component->AddDefault(entity);
		}
	}
}

inline void DeserializeEntityValueComponents(
	const SerializedComponentMap& components,
	Entity entity
) {
	for (const auto& [name, component_json] : components) {
		const auto* component{
			ComponentRegistry::Find(std::string_view{ name })
		};

		if (!component ||
			IsEntityMetadataComponent(*component) ||
			component->is_empty ||
			!component->deserializable) {
			continue;
		}

		component->Deserialize(component_json, entity);
	}
}

} // namespace impl

/// @brief Resolves an entity in a serialized hierarchy by child-index path.
///
/// An empty path resolves the root. Each subsequent index selects a child of
/// the previously resolved entity.
[[nodiscard]] inline SerializedEntity* ResolveSerializedEntity(
	SerializedEntity& root,
	std::span<const std::size_t> path
) {
	SerializedEntity* current{ std::addressof(root) };

	for (const std::size_t index : path) {
		if (index >= current->children.size()) {
			return nullptr;
		}

		current = std::addressof(current->children[index]);
	}

	return current;
}

[[nodiscard]] inline const SerializedEntity* ResolveSerializedEntity(
	const SerializedEntity& root,
	std::span<const std::size_t> path
) {
	const SerializedEntity* current{ std::addressof(root) };

	for (const std::size_t index : path) {
		if (index >= current->children.size()) {
			return nullptr;
		}

		current = std::addressof(current->children[index]);
	}

	return current;
}

/// @brief Serializes an entity's persistent ECS data and optionally its children.
[[nodiscard]] inline SerializedEntity SerializeEntity(
	Entity entity,
	SerializeEntityOptions options = {}
) {
	PTGN_ASSERT(entity, "Cannot serialize a null entity");
	PTGN_ASSERT(entity.Has<Tag>(), "Serialized entity must have a Tag component");

	SerializedEntity output;

	if (options.include_uuid) {
		PTGN_ASSERT(
			entity.Has<UUID>(),
			"Serialized entity must have a UUID component"
		);

		output.uuid = entity.Get<UUID>();
	}

	output.tag = entity.Get<Tag>().value;
	output.tags = impl::SerializeEntityTagComponents(entity);
	output.components = impl::SerializeEntityValueComponents(entity);

	if (!options.include_children || !HasChildren(entity)) {
		return output;
	}

	auto children{ GetChildren(entity) };
	SortByLocalDepth(children);

	output.children.reserve(children.size());

	for (Entity child : children) {
		output.children.emplace_back(
			SerializeEntity(child, options)
		);
	}

	return output;
}

/// @brief Applies serialized identity-independent data to an existing entity.
///
/// This applies Tag, empty marker components, and ordinary components. It does
/// not modify UUID or hierarchy because entity creation and hierarchy restoration
/// require context from scenes, prefabs, or editor commands.
inline void DeserializeEntity(
	const SerializedEntity& input,
	Entity entity
) {
	PTGN_ASSERT(entity, "Cannot deserialize into a null entity");

	if (entity.Has<Tag>()) {
		entity.Get<Tag>().value = input.tag;
	} else {
		entity.Add<Tag>(input.tag);
	}

	impl::DeserializeEntityTagComponents(input.tags, entity);
	impl::DeserializeEntityValueComponents(input.components, entity);
}

/// @return The required persistent UUID of serialized scene/snapshot data.
[[nodiscard]] inline UUID GetSerializedEntityUUID(
	const SerializedEntity& entity
) {
	PTGN_ASSERT(
		entity.uuid.has_value(),
		"Serialized entity does not contain a UUID"
	);

	return *entity.uuid;
}

} // namespace ptgn
