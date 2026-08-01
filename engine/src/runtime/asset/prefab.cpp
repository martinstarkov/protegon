#include "runtime/asset/prefab.h"

#include <cctype>
#include <string>
#include <utility>

#include "core/assert.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/entity_serialization.h"
#include "runtime/ecs/relatives.h"
#include "runtime/ecs/tag.h"
#include "runtime/scene/scene.h"
#include "serialization/json/json_file.h"

namespace ptgn {

namespace {

Entity InstantiatePrefabEntity(
	Scene& scene,
	const SerializedEntity& definition,
	Entity parent
) {
	// Deliberately ignores definition.uuid. Every prefab instance receives
	// a fresh UUID from Scene::CreateEntity.
	Entity entity{
		scene.CreateEntity(
			Tag{ definition.tag }
		)
	};

	DeserializeEntity(
		definition,
		entity
	);

	if (parent) {
		SetParent(
			entity,
			parent
		);
	}

	for (const auto& child :
		 definition.children) {
		InstantiatePrefabEntity(
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
	if (impl::IsEntityMetadataComponent(component) ||
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

	return Prefab{
		.key = std::move(key),
		.root = SerializeEntity(
			entity,
			{
				.include_uuid = false,
				.include_children = include_children,
			}
		),
	};
}

Entity InstantiatePrefab(
	Scene& scene,
	const Prefab& prefab
) {
	Entity root{
		InstantiatePrefabEntity(
			scene,
			prefab.root,
			{}
		)
	};

	scene.Refresh();

	return root;
}

Prefab LoadPrefabFile(
	const path& file_path
) {
	Prefab prefab;
	LoadJson(file_path).get_to(prefab);
	return prefab;
}

void SavePrefabFile(
	const path& file_path,
	const Prefab& prefab
) {
	EnsureDirectory(
		file_path.parent_path()
	);

	json value = prefab;
	SaveJson(value, file_path);
}

std::string MakePrefabSlug(
	std::string_view value
) {
	std::string output;
	output.reserve(value.size());

	bool separator_pending{ false };

	for (char c : value) {
		const auto character{
			static_cast<unsigned char>(c)
		};

		if (std::isalnum(character)) {
			if (separator_pending &&
				!output.empty()) {
				output.push_back('_');
			}

			separator_pending = false;

			output.push_back(
				static_cast<char>(
					std::tolower(character)
				)
			);
		} else {
			separator_pending = true;
		}
	}

	while (!output.empty() &&
		   output.back() == '_') {
		output.pop_back();
	}

	return output.empty()
		? "prefab"
		: output;
}

PrefabKey MakePrefabKey(
	std::string_view value
) {
	if (value.starts_with(
			kPrefabKeyPrefix
		)) {
		value.remove_prefix(
			kPrefabKeyPrefix.size()
		);
	}

	return PrefabKey{
		std::string{ kPrefabKeyPrefix } +
		MakePrefabSlug(value)
	};
}

path GetPrefabSourcePath(
	const PrefabKey& key
) {
	std::string key_value{
		key.value
	};

	if (key_value.starts_with(
			kPrefabKeyPrefix
		)) {
		key_value.erase(
			0,
			kPrefabKeyPrefix.size()
		);
	}

	return path{ kPrefabDirectory } /
		path{
			MakePrefabSlug(key_value) +
			std::string{ kPrefabExtension }
		};
}

path GetPrefabFilePath(
	const path& project_root,
	const PrefabKey& key
) {
	return project_root /
		   GetPrefabSourcePath(key);
}

} // namespace ptgn