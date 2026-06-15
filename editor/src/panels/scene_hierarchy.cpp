#include "panels/scene_hierarchy.h"

#include <imgui.h>

#include <algorithm>
#include <compare>
#include <vector>

#include "core/editor.h"
#include "core/editor_context.h"
#include "panels/scene_list.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/graphics/draw.h"
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

	auto sort_by_depth = [](std::vector<Entity>& entities) {
		std::ranges::stable_sort(entities, [](Entity lhs, Entity rhs) {
			return GetDepth(lhs) < GetDepth(rhs);
		});
	};

	auto draw_entity = [&](auto&& self, Entity entity, std::size_t recursion_depth) -> void {
		if (recursion_depth > kMaxParentDepth) {
			PTGN_ERROR(
				"Maximum parent depth exceeded while sorting entities by depth. "
				"This likely indicates a cycle in the entity hierarchy."
			);
			return;
		}

		bool selected{ entity == selected_entity_ };
		bool has_children{ HasChildren(entity) };

		ImGui::PushID(static_cast<int>(entity.GetUUID()));

		ImGuiTreeNodeFlags flags{ ImGuiTreeNodeFlags_OpenOnArrow |
								  ImGuiTreeNodeFlags_OpenOnDoubleClick |
								  ImGuiTreeNodeFlags_SpanAvailWidth |
								  ImGuiTreeNodeFlags_DefaultOpen };

		if (selected) {
			flags |= ImGuiTreeNodeFlags_Selected;
		}

		if (!has_children) {
			flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		}

		auto label{ entity.GetTag() };
		bool open{ ImGui::TreeNodeEx("##Entity", flags, "%s", label.c_str()) };

		if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
			selected_entity_ = entity;
		}

		if (ImGui::BeginPopupContextItem()) {
			if (ImGui::MenuItem("Delete")) {
				// TODO: Delete entity.
			}

			ImGui::EndPopup();
		}

		if (has_children && open) {
			auto children{ GetChildren(entity) };
			sort_by_depth(children);

			for (auto child : children) {
				self(self, child, recursion_depth + 1);
			}

			ImGui::TreePop();
		}

		ImGui::PopID();
	};

	std::vector<Entity> roots;

	for (auto entity : selected_scene->Entities()) {
		if (!HasParent(entity)) {
			roots.emplace_back(entity);
		}
	}

	sort_by_depth(roots);

	for (auto entity : roots) {
		draw_entity(draw_entity, entity, 0);
	}

	ImGui::End();
}

Entity SceneHierarchyPanel::GetSelectedEntity() const {
	return selected_entity_;
}

void SceneHierarchyPanel::SetSelectedEntity(Entity entity) {
	selected_entity_ = entity;
}

} // namespace ptgn::editor