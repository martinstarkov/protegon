#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "runtime/ecs/entity.h"
#include "runtime/ecs/uuid.h"
#include "serialization/serialize.h"

namespace ptgn {

class Scene;

enum class SceneLayerKind : std::uint8_t {
	Entity,
	Tile,
};
PTGN_REFLECT_ENUM(SceneLayerKind);

/// @brief Stable scene-local identity for an authoring layer.
///
/// Layer ids are deliberately independent from ECS entity ids/UUIDs: a layer is scene metadata,
/// not an entity. This keeps layer membership out of prefab captures while allowing Tilemap and
/// PaintGenerator themselves to remain ordinary ECS entities.
struct SceneLayerId {
	std::uint64_t value{};

	constexpr explicit operator bool() const {
		return value != 0;
	}

	constexpr auto operator<=>(const SceneLayerId&) const = default;

	PTGN_REFLECT_VALUE(SceneLayerId, value)
};

struct SceneLayer {
	SceneLayerId id{};
	std::string name{ "Layer" };
	SceneLayerKind kind{ SceneLayerKind::Entity };
	bool visible{ true };
	bool locked{ false };
	bool selectable{ true };

	bool operator==(const SceneLayer&) const = default;

	PTGN_REFLECT(SceneLayer, id, name, kind, visible, locked, selectable)
};

struct SceneLayerUUIDHasher {
	std::size_t operator()(const UUID& uuid) const noexcept {
		return static_cast<std::size_t>(uuid);
	}
};

struct SerializedSceneLayerMembership {
	UUID entity{};
	SceneLayerId layer{};

	constexpr bool operator==(const SerializedSceneLayerMembership&) const = default;

	PTGN_REFLECT(SerializedSceneLayerMembership, entity, layer)
};

/// @brief Persistent representation of the scene's layer model.
///
/// Membership is serialized separately from entity components on purpose. Scene layers are
/// scene-local organization, so prefab captures must not carry a source scene's layer id.
struct SerializedSceneLayers {
	std::vector<SceneLayer> layers{};
	std::vector<SerializedSceneLayerMembership> memberships{};
	SceneLayerId default_entity_layer{};
	std::uint64_t next_layer_id{ 1 };

	PTGN_REFLECT(
		SerializedSceneLayers,
		layers,
		memberships,
		default_entity_layer,
		next_layer_id
	)
};

/// @brief Scene-owned layer registry and entity-to-layer membership table.
///
/// Rules:
/// - Every live entity belongs to exactly one layer.
/// - New generic entities begin in the automatically-created default Entity layer.
/// - Entity layers may contain any entity except Tilemap entities.
/// - Tile layers may contain only Tilemap and PaintGenerator entities.
/// - PaintGenerator entities are valid in either layer kind.
///
/// Parenting is orthogonal to layers. Editor reparenting across layers should call Assign() for
/// the moved subtree as part of the same operation so every descendant remains in one layer.
class SceneLayers {
public:
	SceneLayers();

	void Reset();

	[[nodiscard]] const std::vector<SceneLayer>& GetLayers() const;
	[[nodiscard]] SceneLayerId GetDefaultEntityLayer() const;

	[[nodiscard]] SceneLayer* Find(SceneLayerId id);
	[[nodiscard]] const SceneLayer* Find(SceneLayerId id) const;

	[[nodiscard]] SceneLayerId Create(
		SceneLayerKind kind,
		std::string name = {}
	);

	/// @brief Deletes a non-default layer.
	///
	/// If the layer contains entities, replacement must identify another layer capable of
	/// containing every member. Empty layers can be deleted without a replacement.
	bool Delete(
		Scene& scene,
		SceneLayerId id,
		std::optional<SceneLayerId> replacement = std::nullopt
	);

	bool Move(SceneLayerId id, std::size_t new_index);

	bool Rename(SceneLayerId id, std::string_view name);
	bool SetVisible(SceneLayerId id, bool visible);
	bool SetLocked(SceneLayerId id, bool locked);
	bool SetSelectable(SceneLayerId id, bool selectable);

	[[nodiscard]] std::optional<SceneLayerId> GetLayerId(UUID entity) const;
	[[nodiscard]] std::optional<SceneLayerId> GetLayerId(Entity entity) const;
	[[nodiscard]] SceneLayer* GetLayer(Entity entity);
	[[nodiscard]] const SceneLayer* GetLayer(Entity entity) const;

	/// @brief Returns root entities belonging to the requested layer. Child entities remain under
	/// their normal parent in hierarchy UIs instead of being duplicated at layer level.
	[[nodiscard]] std::vector<Entity> GetRootEntities(const Scene& scene, SceneLayerId layer) const;

	/// @return Whether the entity's current type is permitted by the destination layer kind.
	[[nodiscard]] bool CanAssign(Entity entity, SceneLayerId layer) const;

	/// @brief Assigns an entity to a layer. By default the full child subtree is assigned too.
	/// @return False without changing anything if the destination does not exist or any entity in
	/// the requested subtree is incompatible with the destination layer.
	bool Assign(Entity entity, SceneLayerId layer, bool include_children = true);

	/// @brief Called by Scene whenever a generic entity is created. Existing serialized membership
	/// is preserved; otherwise the entity is placed in the default Entity layer.
	void RegisterEntity(Entity entity);
	void UnregisterEntity(UUID entity);

	/// @brief Removes memberships for dead entities and supplies the default layer for any live
	/// entity that has no entry. This is safe to call after ECS refreshes.
	void Prune(Scene& scene);

	[[nodiscard]] bool IsVisible(Entity entity) const;
	[[nodiscard]] bool IsLocked(Entity entity) const;
	[[nodiscard]] bool IsSelectable(Entity entity) const;

	[[nodiscard]] SerializedSceneLayers Serialize(const Scene& scene) const;
	void Deserialize(const SerializedSceneLayers& serialized);

	/// @brief Performs structural validation after scene deserialization or before saving.
	[[nodiscard]] bool Validate(const Scene& scene) const;

private:
	[[nodiscard]] bool IsAllowed(Entity entity, SceneLayerKind kind) const;
	void CollectSubtree(Entity entity, std::vector<Entity>& output) const;

	std::vector<SceneLayer> layers_{};
	std::unordered_map<UUID, SceneLayerId, SceneLayerUUIDHasher> memberships_{};
	SceneLayerId default_entity_layer_{};
	std::uint64_t next_layer_id_{ 1 };
};

} // namespace ptgn

template <>
struct std::hash<ptgn::SceneLayerId> {
	std::size_t operator()(const ptgn::SceneLayerId& id) const noexcept {
		return static_cast<std::size_t>(id.value);
	}
};
