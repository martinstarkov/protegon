#include "runtime/asset/prefab.h"

#include <algorithm>
#include <cctype>
#include <ranges>
#include <string>
#include <utility>

#include "core/assert.h"
#include "runtime/ecs/relatives.h"
#include "runtime/graphics/draw.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/tag.h"
#include "runtime/scene/scene.h"
#include "serialization/json/json_file.h"
#include "runtime/ecs/entity_serialization.h"

namespace ptgn {

namespace {

[[nodiscard]] PrefabEntity CapturePrefabEntity(
	Entity entity,
	bool include_children
) {
	PrefabEntity output;

	if (entity.Has<Tag>()) {
		output.tag = entity.Get<Tag>().value;
	}

	json serialized_tags = SerializeEntityTags(entity);

	for (const auto& serialized_tag : serialized_tags) {
		output.tags.emplace_back(
			serialized_tag.get<std::string>()
		);
	}

	json serialized_components = SerializeEntityComponents(entity);

	for (auto& [name, value] : serialized_components.items()) {
		output.components.insert_or_assign(
			name,
			std::move(value)
		);
	}

	if (!include_children ||
		!HasChildren(entity)) {
		return output;
	}

	auto children{ GetChildren(entity) };
	SortByLocalDepth(children);

	output.children.reserve(
		children.size()
	);

	for (Entity child : children) {
		output.children.emplace_back(
			CapturePrefabEntity(
				child,
				true
			)
		);
	}

	return output;
}

[[nodiscard]] Entity InstantiatePrefabEntity(
	Scene& scene,
	const PrefabEntity& definition,
	Entity parent
) {
	auto entity{
		scene.CreateEntity(
			Tag{ definition.tag }
		)
	};

	json serialized_tags = definition.tags;

	DeserializeEntityTags(
		serialized_tags,
		entity
	);

	json serialized_components = definition.components;

	DeserializeEntityComponents(
		serialized_components,
		entity
	);

	if (parent) {
		SetParent(
			entity,
			parent
		);
	}

	for (const auto& child : definition.children) {
		(void)InstantiatePrefabEntity(
			scene,
			child,
			entity
		);
	}

	return entity;
}

} // namespace

bool IsPrefabComponentSupported(
	const RegisteredComponent& component
) {
	if (impl::IsSceneMetadataComponent(component) ||
		!component.has) {
		return false;
	}

	if (component.is_empty) {
		return component.add_default != nullptr;
	}

	return component.serializable &&
		component.deserializable &&
		component.serialize &&
		component.deserialize;
}

Prefab CapturePrefab(
	Entity entity,
	PrefabKey key,
	bool include_children
) {
	PTGN_ASSERT(
		entity,
		"Cannot capture a null entity as a prefab"
	);

	PTGN_ASSERT(
		!key.value.empty(),
		"Prefab key cannot be empty"
	);

	Prefab prefab;
	prefab.key = std::move(key);
	prefab.root = CapturePrefabEntity(
		entity,
		include_children
	);

	return prefab;
}

Entity InstantiatePrefab(Scene& scene, const Prefab& prefab) {
	auto root{ InstantiatePrefabEntity(scene, prefab.root, {}) };
	scene.Refresh();
	return root;
}

Prefab LoadPrefabFile(const path& file_path) {
	Prefab prefab;
	LoadJson(file_path).get_to(prefab);
	return prefab;
}

void SavePrefabFile(const path& file_path, const Prefab& prefab) {
	EnsureDirectory(file_path.parent_path());
	json value = prefab;
	SaveJson(value, file_path);
}

std::string MakePrefabSlug(std::string_view value) {
	std::string output;
	output.reserve(value.size());
	bool separator_pending{ false };

	for (auto c : value) {
		if (std::isalnum(c)) {
			if (separator_pending && !output.empty()) {
				output.push_back('_');
			}
			separator_pending = false;
			output.push_back(static_cast<char>(std::tolower(c)));
		} else {
			separator_pending = true;
		}
	}

	while (!output.empty() && output.back() == '_') {
		output.pop_back();
	}

	return output.empty() ? "prefab" : output;
}

PrefabKey MakePrefabKey(std::string_view value) {
	if (value.starts_with(kPrefabKeyPrefix)) {
		value.remove_prefix(kPrefabKeyPrefix.size());
	}

	return PrefabKey{ std::string{ kPrefabKeyPrefix } + MakePrefabSlug(value) };
}

path GetPrefabSourcePath(const PrefabKey& key) {
	std::string key_value{ key.value };
	if (key_value.starts_with(kPrefabKeyPrefix)) {
		key_value.erase(0, kPrefabKeyPrefix.size());
	}

	return path{ kPrefabDirectory } /
		path{ MakePrefabSlug(key_value) + std::string{ kPrefabExtension } };
}

path GetPrefabFilePath(const path& project_root, const PrefabKey& key) {
	return project_root / GetPrefabSourcePath(key);
}

} // namespace ptgn
