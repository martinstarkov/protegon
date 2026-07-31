#pragma once

#include <array>
#include <optional>
#include <string>

#include "core/editor_selection.h"
#include "runtime/asset/asset_key.h"
#include "runtime/ecs/entity.h"

namespace ptgn::editor {

class EditorContext;

using SceneHierarchyTab = EditorSelectionMode;

class SceneHierarchyPanel {
public:
	void Bind(EditorContext& ctx);
	void OnRender(EditorContext& ctx);

	[[nodiscard]] Entity GetSelectedEntity() const;
	void SetSelectedEntity(Entity entity, bool undoable = true);

	[[nodiscard]] std::optional<PrefabKey> GetSelectedPrefab() const;
	void SetSelectedPrefab(std::optional<PrefabKey> prefab, bool undoable = true);

	[[nodiscard]] SceneHierarchyTab GetActiveTab() const;

private:
	[[nodiscard]] bool DrawSceneHierarchy(EditorContext& ctx);
	[[nodiscard]] bool DrawPrefabs(EditorContext& ctx);
	void SetActiveTab(SceneHierarchyTab tab);

	EditorContext* context_{ nullptr };

	std::optional<PrefabKey> renaming_prefab_;
	std::string prefab_rename_text_;
	std::string prefab_rename_error_;
	bool focus_prefab_rename_{ false };

	std::array<char, 256> filter_{};
};

} // namespace ptgn::editor
