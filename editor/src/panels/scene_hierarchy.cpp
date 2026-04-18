#include "panels/scene_hierarchy.h"

#include <imgui.h>

#include "core/editor_context.h"

namespace ptgn::editor {

void SceneHierarchyPanel::OnRender(EditorContext& ctx) {
	ImGui::Begin("Scene Hierarchy###SceneHierarchyWindow");
	ImGui::TextUnformatted("Scene hierarchy panel");
	ImGui::End();
}

Entity SceneHierarchyPanel::GetSelectedEntity() const {
	return selected_entity_;
}

void SceneHierarchyPanel::SetSelectedEntity(Entity entity) {}

} // namespace ptgn::editor