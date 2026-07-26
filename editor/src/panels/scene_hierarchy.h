#pragma once

#include <array>
#include <optional>
#include <string>

#include "runtime/asset/asset_key.h"
#include "runtime/ecs/entity.h"

namespace ptgn::editor {

class EditorContext;

enum class SceneHierarchyTab {
	SceneHierarchy,
	Prefabs
};

class SceneHierarchyPanel {
public:
	void OnRender(EditorContext& ctx);

	[[nodiscard]] Entity GetSelectedEntity() const;
	void SetSelectedEntity(Entity entity);

	[[nodiscard]] const std::optional<PrefabKey>& GetSelectedPrefab() const;
	void SetSelectedPrefab(std::optional<PrefabKey> prefab);

	[[nodiscard]] SceneHierarchyTab GetActiveTab() const;

private:
	[[nodiscard]] bool DrawSceneHierarchy(EditorContext& ctx);
	[[nodiscard]] bool DrawPrefabs(EditorContext& ctx);

	Entity selected_entity_;
	std::optional<PrefabKey> selected_prefab_;
	SceneHierarchyTab active_tab_{ SceneHierarchyTab::SceneHierarchy };

	std::optional<PrefabKey> renaming_prefab_;
	std::string prefab_rename_text_;
	std::string prefab_rename_error_;
	bool focus_prefab_rename_{ false };

	std::array<char, 256> filter_{};
};

} // namespace ptgn::editor
