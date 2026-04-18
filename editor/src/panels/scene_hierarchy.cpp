#include "panels/scene_hierarchy.h"

#include <imgui.h>

#include "core/editor.h"
#include "core/editor_context.h"
#include "panels/scene_list.h"
#include "runtime/scene/scene.h"

namespace ptgn::editor {

void SceneHierarchyPanel::OnRender(EditorContext& ctx) {
	ImGui::Begin("Scene Hierarchy###SceneHierarchyWindow");

	const auto& scene_list{ ctx.editor.GetSceneListPanel() };

	auto selected_scene{ scene_list.GetSelectedScene() };

	if (!selected_scene) {
		ImGui::End();
		return;
	}

	const auto& entities{ selected_scene->Entities() };

	for (auto entity : entities) {
		bool selected{ entity == selected_entity_ };

		auto label{ entity.GetTag() };

		if (ImGui::Selectable(label.c_str(), selected)) {
			selected_entity_ = entity;
		}

		if (ImGui::BeginPopupContextItem()) {
			if (ImGui::MenuItem("Delete")) {
				// TODO: Fix.
				// ctx.editor.DeleteScene(i);
				ImGui::EndPopup();
				break;
			}
			ImGui::EndPopup();
		}
	}

	ImGui::End();
}

Entity SceneHierarchyPanel::GetSelectedEntity() const {
	return selected_entity_;
}

void SceneHierarchyPanel::SetSelectedEntity(Entity entity) {}

} // namespace ptgn::editor