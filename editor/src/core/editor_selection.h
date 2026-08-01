#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "runtime/asset/asset_key.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_serialization.h"
#include "runtime/ecs/uuid.h"
#include "serialization/serialize.h"

namespace ptgn {

class Scene;

namespace editor {

class Editor;
class EditorContext;

enum class EditorSelectionMode {
	SceneHierarchy,
	Prefabs
};

struct SceneEntitySelection {
	std::string scene_key;
	bool runtime{ false };
	std::optional<UUID> entity_uuid;

	bool operator==(const SceneEntitySelection&) const = default;

	PTGN_REFLECT(SceneEntitySelection, scene_key, runtime, entity_uuid)
};

struct EditorSelection {
	std::string selected_scene_key;
	bool selected_scene_runtime{ false };
	std::vector<SceneEntitySelection> scene_entities;

	/// Selected prefab asset. The selected entity inside the prefab is stored
	/// separately as a child-index path from the prefab root.
	std::optional<PrefabKey> selected_prefab;
	SerializedEntityPath selected_prefab_entity_path;

	EditorSelectionMode mode{ EditorSelectionMode::SceneHierarchy };

	void Clear();

	[[nodiscard]] bool HasEntitySelection(
		std::string_view scene_key,
		bool runtime
	) const;

	[[nodiscard]] std::optional<UUID> GetEntityUUID(
		std::string_view scene_key,
		bool runtime
	) const;

	void SetEntityUUID(
		std::string scene_key,
		bool runtime,
		std::optional<UUID> entity_uuid
	);

	void RemoveScene(std::string_view scene_key);
	void RenameScene(std::string_view old_key, std::string_view new_key);

	bool operator==(const EditorSelection&) const = default;

	PTGN_REFLECT(
		EditorSelection,
		selected_scene_key,
		selected_scene_runtime,
		scene_entities,
		selected_prefab,
		selected_prefab_entity_path,
		mode
	)
};

[[nodiscard]] Scene* ResolveSelectedScene(EditorContext& ctx);
[[nodiscard]] const Scene* ResolveSelectedScene(const EditorContext& ctx);
[[nodiscard]] Entity ResolveSelectedEntity(EditorContext& ctx);

/// Applies selection without adding an undo command.
void ApplyEditorSelection(EditorContext& ctx, EditorSelection selection);

/// Applies selection and records it in the shared undo stack.
bool SetEditorSelection(
	EditorContext& ctx,
	EditorSelection selection,
	std::string label
);

} // namespace editor

} // namespace ptgn
