#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "core/util/file.h"
#include "runtime/asset/asset_key.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_serialization.h"
#include "serialization/serialize.h"

namespace ptgn {

class Scene;
struct RegisteredComponent;

inline constexpr std::string_view kPrefabDirectory{ "assets/prefabs" };
inline constexpr std::string_view kPrefabExtension{ ".ptgnprefab" };
inline constexpr std::string_view kPrefabKeyPrefix{ "prefabs/" };

struct Prefab {
	PrefabKey key{};

	/// @brief UUID is absent for prefab captures. Instantiation always creates fresh UUIDs for the
	/// complete hierarchy.
	SerializedEntity root{};

	PTGN_REFLECT(Prefab, key, root)
};

/// @brief Persistent editor-scene link from an instantiated entity back to its prefab definition.
///
/// Every entity in a linked prefab hierarchy receives this component. `entity_path` is empty for
/// the instance root and identifies the corresponding serialized prefab child for descendants.
/// Runtime scenes remove these links after synchronizing the hierarchy, leaving ordinary entities.
struct PrefabInstance {
	PrefabKey prefab{};
	SerializedEntityPath entity_path{};

	bool operator==(const PrefabInstance&) const = default;

	PTGN_REFLECT(PrefabInstance, prefab, entity_path)
};

enum class PrefabInstantiationMode : std::uint8_t {
	/// Editor scene -> linked. Runtime scene -> baked.
	Auto,
	Linked,
	Baked,
};

[[nodiscard]] bool IsPrefabComponentSupported(const RegisteredComponent& component);

[[nodiscard]] Prefab CapturePrefab(
	Entity entity,
	PrefabKey key,
	bool include_children = true
);

/// @brief Creates a prefab hierarchy. Auto keeps editor instances linked and creates ordinary
/// entities directly in runtime scenes.
[[nodiscard]] Entity InstantiatePrefab(
	Scene& scene,
	const Prefab& prefab,
	PrefabInstantiationMode mode = PrefabInstantiationMode::Auto
);

[[nodiscard]] bool IsPrefabInstance(Entity entity);
[[nodiscard]] bool IsPrefabInstanceRoot(Entity entity);
[[nodiscard]] Entity GetPrefabInstanceRoot(Entity entity);
[[nodiscard]] const PrefabInstance* GetPrefabInstance(Entity entity);

/// @brief Replaces prefab-authored data on one linked instance while preserving the instance root
/// transform, parent, UUID and layer placement.
[[nodiscard]] bool SyncPrefabInstance(Entity instance_root, const Prefab& prefab);

/// @brief Resolves the linked prefab asset and synchronizes one instance.
[[nodiscard]] bool SyncPrefabInstance(Entity instance_root);

/// @brief Synchronizes every linked instance of one prefab in a scene.
std::size_t SyncPrefabInstances(Scene& scene, const PrefabKey& key);

/// @brief Synchronizes every linked prefab instance in a scene.
std::size_t SyncPrefabInstances(Scene& scene);

/// @brief Changes linked instances from one prefab key to another, then synchronizes them.
std::size_t RetargetPrefabInstances(
	Scene& scene,
	const PrefabKey& old_key,
	const PrefabKey& new_key
);

/// @brief Synchronizes an instance one final time and removes all PrefabInstance metadata.
[[nodiscard]] bool BakePrefabInstance(Entity instance_root);
[[nodiscard]] bool BakePrefabInstance(Entity instance_root, const Prefab& prefab);

/// @brief Bakes all linked prefab instances in the scene. Runtime scene initialization calls this.
std::size_t BakePrefabInstances(Scene& scene);

[[nodiscard]] Prefab LoadPrefabFile(const path& file_path);
void SavePrefabFile(const path& file_path, const Prefab& prefab);

[[nodiscard]] std::string MakePrefabSlug(std::string_view value);
[[nodiscard]] PrefabKey MakePrefabKey(std::string_view value);
[[nodiscard]] path GetPrefabSourcePath(const PrefabKey& key);
[[nodiscard]] path GetPrefabFilePath(const path& project_root, const PrefabKey& key);

} // namespace ptgn
