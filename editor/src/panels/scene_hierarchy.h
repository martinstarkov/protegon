#pragma once

#include <array>
#include <optional>
#include <string>

#include "editor/editor_selection.h"
#include "runtime/asset/asset_key.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_serialization.h"

namespace ptgn {

class Scene;

} // namespace ptgn

namespace ptgn::editor {

class EditorContext;

// Same assignment restrictions for hierarchy drag/drop and the Rendering inspector.
[[nodiscard]] bool CanReparentHierarchyEntity(Entity entity, Entity parent);
[[nodiscard]] bool CanAssignEntityToGroup(Entity entity, Entity layer);

using SceneHierarchyTab = EditorSelectionMode;

class SceneHierarchyPanel {
public:
	void Bind(EditorContext& ctx);
	void OnRender(EditorContext& ctx);

	[[nodiscard]] Entity GetSelectedEntity() const;
	void SetSelectedEntity(Entity entity, bool undoable = true);

	[[nodiscard]] std::optional<PrefabKey> GetSelectedPrefab() const;
	[[nodiscard]] const SerializedEntityPath& GetSelectedPrefabEntityPath() const;

	void SetSelectedPrefab(std::optional<PrefabKey> prefab, bool undoable = true);
	void SetSelectedPrefab(
		std::optional<PrefabKey> prefab, SerializedEntityPath entity_path, bool undoable = true
	);

	[[nodiscard]] SceneHierarchyTab GetActiveTab() const;
	void SetActiveTab(SceneHierarchyTab tab);

	/// Whether Tiles is the visible dock window in the current frame.
	[[nodiscard]] bool IsTilesWindowVisible() const {
		return tiles_window_visible_;
	}

private:
	bool DrawSceneHierarchy(EditorContext& ctx);
	bool DrawPrefabs(EditorContext& ctx);
	bool DrawTiles(EditorContext& ctx);

	EditorContext* context_{ nullptr };
	bool tiles_window_visible_{ false };

	std::optional<SceneEntitySelection> renaming_entity_;
	std::string entity_rename_text_;
	std::string entity_rename_error_;
	bool focus_entity_rename_{ false };

	std::optional<PrefabKey> renaming_prefab_;
	std::string prefab_rename_text_;
	std::string prefab_rename_error_;
	bool focus_prefab_rename_{ false };

	std::optional<PrefabKey> force_open_prefab_;
	SerializedEntityPath force_open_prefab_entity_path_;

	std::array<char, 256> filter_{};
};

} // namespace ptgn::editor
