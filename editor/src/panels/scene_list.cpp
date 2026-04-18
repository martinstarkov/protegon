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

namespace ptgn::editor {

void SceneListPanel::OnRender(EditorContext& ctx) {
	ImGui::Begin("Scenes");

	const auto& scenes{ ctx.editor.GetScenes() };

	for (std::size_t i{ 0 }; i < scenes.size(); i++) {
		const auto& scene{ scenes[i] };

		bool selected{ scene.get() == selected_scene_ };

		if (ImGui::Selectable(scene->GetTag().c_str(), selected)) {
			selected_scene_ = scene.get();
			auto& scene_hierarchy{ ctx.editor.GetSceneHierarchyPanel() };

			const auto& entities{ scene->Entities() };

			entities
				.view

					scene_hierarchy.SetSelectedEntity();
			app.selected_entity_id_ =
				app.CurrentScene().entities.empty() ? -1 : app.CurrentScene().entities.front().id;
			app.selected_component_ = ComponentKind::Transform;
		}
	}

	for (int i = 0; i < static_cast<int>(app.scenes_.size()); ++i) {
		const bool selected = (app.current_scene_index_ == i);
		if (ImGui::Selectable(app.scenes_[i].name.c_str(), selected)) {
			app.current_scene_index_ = i;
			app.selected_entity_id_ =
				app.CurrentScene().entities.empty() ? -1 : app.CurrentScene().entities.front().id;
			app.selected_component_ = ComponentKind::Transform;
		}

		if (ImGui::BeginPopupContextItem()) {
			if (ImGui::MenuItem("Rename")) {
				app.BeginRenameSceneInline(i);
			}
			if (ImGui::MenuItem("Delete")) {
				app.DeleteScene(i);
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
			SceneData scene;
			scene.name = "NewScene_" + std::to_string(static_cast<int>(app.scenes_.size()) + 1);
			scene.entities.push_back(app.MakeEntity(
				app.next_entity_id_++, -1, "Main Camera", true, false, true, false, false
			));
			app.scenes_.push_back(scene);
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