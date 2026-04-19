#include "panels/scene_list.h"

#include <imgui.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "app/application.h"
#include "core/editor.h"
#include "core/editor_context.h"
#include "core/util/file.h"
#include "runtime/ecs/entity.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_registry.h"
#include "runtime/scene/scene_view.h"

namespace ptgn::editor {

std::optional<SceneEditorState> MakeSceneEditorState(std::string_view name) {
	auto& registry{ impl::GetSceneRegistry() };
	auto it = registry.find(name);
	if (it != registry.end()) {
		const auto& desc{ it->second };
		return SceneEditorState{ .scene_type_name = desc.display_name,
								 .params		  = desc.default_params() };
	} else {
		return std::nullopt;
		// TODO: Re-enable.
		// PTGN_ERROR("Scene not found in registry: ", name);
	}
}

// --------------------------------------------------
// Generic ImGui JSON editor
// --------------------------------------------------

static void DrawJsonEditor(const char* label, json& value) {
	if (value.is_boolean()) {
		bool v = value.get<bool>();
		if (ImGui::Checkbox(label, &v)) {
			value = v;
		}
	} else if (value.is_number_integer()) {
		int v = value.get<int>();
		if (ImGui::InputInt(label, &v)) {
			value = v;
		}
	} else if (value.is_number_float()) {
		float v = value.get<float>();
		if (ImGui::InputFloat(label, &v)) {
			value = v;
		}
	} else if (value.is_string()) {
		std::string s = value.get<std::string>();
		char buf[256]{};
		std::snprintf(buf, sizeof(buf), "%s", s.c_str());
		if (ImGui::InputText(label, buf, sizeof(buf))) {
			value = std::string(buf);
		}
	} else if (value.is_object()) {
		if (ImGui::TreeNode(label)) {
			for (auto it = value.begin(); it != value.end(); ++it) {
				DrawJsonEditor(it.key().c_str(), it.value());
			}
			ImGui::TreePop();
		}
	} else if (value.is_array()) {
		if (ImGui::TreeNode(label)) {
			for (int i = 0; i < static_cast<int>(value.size()); ++i) {
				std::string item = "[" + std::to_string(i) + "]";
				DrawJsonEditor(item.c_str(), value[i]);
			}
			ImGui::TreePop();
		}
	} else {
		ImGui::TextDisabled("%s: unsupported", label);
	}
}

void SceneListPanel::DrawSceneParamUI(EditorContext& ctx) {
	if (!state_.has_value()) {
		state_ = MakeSceneEditorState("EditorScene");
	}

	if (!state_.has_value()) {
		return;
	}

	ImGui::TextUnformatted(state_->scene_type_name.c_str());
	ImGui::Separator();

	{
		char buf[256]{};
		std::snprintf(buf, sizeof(buf), "%s", "Title Hello");
		if (ImGui::InputText("Title", buf, sizeof(buf))) {
			// state_->title = std::string(buf);
		}
	}

	for (auto it = state_->params.begin(); it != state_->params.end(); ++it) {
		DrawJsonEditor(it.key().c_str(), it.value());
	}

	if (ImGui::Button("Enter Scene")) {
		std::string scene_tag{ "" };
		ctx.editor.GetSceneManager().PushCommand(
			impl::SceneManager::CommandType::ReEnter, scene_tag, Hash(scene_tag),
			SceneTransitionPriority{}, impl::GetSceneFactory("EditorScene", state_->params),
			nullptr, nullptr
		);
	}
}

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

	DrawSceneParamUI(ctx);

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