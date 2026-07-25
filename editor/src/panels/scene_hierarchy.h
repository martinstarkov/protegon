#pragma once

#include <array>
#include <optional>

#include "runtime/asset/asset_key.h"
#include "runtime/ecs/entity.h"

namespace ptgn::editor {

class EditorContext;

enum class SceneHierarchyTab {
	Entities,
	Prefabs
};

class SceneHierarchyPanel {
public:
	void OnRender(EditorContext& ctx);

	[[nodiscard]] Entity GetSelectedEntity() const;
	void SetSelectedEntity(Entity entity);

	[[nodiscard]] const std::optional<PrefabKey>& GetSelectedPrefab() const;
	void SetSelectedPrefab(std::optional<PrefabKey> prefab);

private:
	Entity selected_entity_;
	std::optional<PrefabKey> selected_prefab_;
	SceneHierarchyTab active_tab_{ SceneHierarchyTab::Entities };
	bool select_active_tab_{ false };
	std::array<char, 256> filter_{};
};

} // namespace ptgn::editor
