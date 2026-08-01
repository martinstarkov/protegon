#pragma once

#include <string>
#include <string_view>
#include <utility>

#include "core/util/file.h"
#include "runtime/asset/asset_key.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_serialization.h"
#include "serialization/serialize.h"

namespace ptgn {

class Scene;
struct RegisteredComponent;

inline constexpr std::string_view kPrefabDirectory{
	"Prefabs"
};

inline constexpr std::string_view kPrefabExtension{
	".ptgnprefab"
};

inline constexpr std::string_view kPrefabKeyPrefix{
	"prefabs/"
};

struct Prefab {
	PrefabKey key;

	/// UUID is absent for prefab captures. Instantiation always creates
	/// fresh UUIDs for the complete hierarchy.
	SerializedEntity root;

	PTGN_REFLECT(
		Prefab,
		key,
		root
	)
};

/// @return Whether a registered component can be represented in a prefab asset.
[[nodiscard]] bool IsPrefabComponentSupported(
	const RegisteredComponent& component
);

/// @brief Captures an entity and, optionally, its complete child hierarchy.
///
/// Persistent UUIDs are deliberately excluded.
[[nodiscard]] Prefab CapturePrefab(
	Entity entity,
	PrefabKey key,
	bool include_children = true
);

/// @brief Creates a new entity hierarchy using fresh UUIDs.
[[nodiscard]] Entity InstantiatePrefab(
	Scene& scene,
	const Prefab& prefab
);

[[nodiscard]] Prefab LoadPrefabFile(
	const path& file_path
);

void SavePrefabFile(
	const path& file_path,
	const Prefab& prefab
);

[[nodiscard]] std::string MakePrefabSlug(
	std::string_view value
);

[[nodiscard]] PrefabKey MakePrefabKey(
	std::string_view value
);

[[nodiscard]] path GetPrefabSourcePath(
	const PrefabKey& key
);

[[nodiscard]] path GetPrefabFilePath(
	const path& project_root,
	const PrefabKey& key
);

} // namespace ptgn