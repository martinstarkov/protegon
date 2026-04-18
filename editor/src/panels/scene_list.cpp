#include "panels/scene_list.h"

#include <imgui.h>

#include <filesystem>
#include <string>
#include <vector>

#include "core/editor.h"
#include "core/editor_context.h"
#include "core/util/file.h"
#include "runtime/ecs/entity.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_view.h"

namespace ptgn::editor {

void SceneListPanel::OnRender(EditorContext& ctx) {
	ImGui::Begin("Scenes");

	auto& scenes{ ctx.editor.GetScenes() };

	for (std::size_t i{ 0 }; i < scenes.size(); i++) {
		const auto& scene{ scenes[i] };

		bool selected{ scene.get() == selected_scene_ };

		auto label{ scene->GetTag().empty() ? "Untitled Scene" : scene->GetTag().c_str() };

		if (ImGui::Selectable(label, selected)) {
			selected_scene_ = scene.get();
			auto& scene_hierarchy{ ctx.editor.GetSceneHierarchyPanel() };

			const auto& entities{ scene->Entities() };

			auto first_entity = entities.Front();

			scene_hierarchy.SetSelectedEntity(first_entity);

			// TODO: Fix.
			// app.selected_component_ = ComponentKind::Transform;
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

	if (ImGui::BeginPopupContextWindow(
			"ScenesContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems
		)) {
		if (ImGui::MenuItem("Add Scene")) {
			// TODO: Fix.
			/*SceneData scene;
			scene.name = "NewScene_" + std::to_string(static_cast<int>(app.scenes_.size()) + 1);
			scene.entities.push_back(app.MakeEntity(
				app.next_entity_id_++, -1, "Main Camera", true, false, true, false, false
			));
			app.scenes_.push_back(scene);*/
		}
		ImGui::EndPopup();
	}

	ImGui::End();
}

Scene* SceneListPanel::GetSelectedScene() const {
	return selected_scene_;
}

void SceneListPanel::SetSelectedScene(Scene* scene, const path& scene_path) {
	selected_scene_		 = scene;
	selected_scene_path_ = scene_path;
}

} // namespace ptgn::editor