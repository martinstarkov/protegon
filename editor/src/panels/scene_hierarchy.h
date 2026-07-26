#pragma once

#include <array>
#include <optional>
#include <string>

#include "runtime/asset/asset_key.h"
#include "runtime/ecs/entity.h"

namespace ptgn::editor {

class EditorContext;

class SceneHierarchyPanel {
public:
	void OnRender(EditorContext& ctx);

	[[nodiscard]] Entity GetSelectedEntity() const;
	void SetSelectedEntity(Entity entity);

	[[nodiscard]] const std::optional<PrefabKey>& GetSelectedPrefab() const;
	void SetSelectedPrefab(std::optional<PrefabKey> prefab);

private:
	void DrawSceneHierarchy(EditorContext& ctx);
	void DrawPrefabs(EditorContext& ctx);

	Entity selected_entity_;
	std::optional<PrefabKey> selected_prefab_;

	std::optional<PrefabKey> renaming_prefab_;
	std::string prefab_rename_text_;
	std::string prefab_rename_error_;
	bool focus_prefab_rename_{ false };

	std::array<char, 256> filter_{};
};

} // namespace ptgn::editor