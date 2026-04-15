#include "protegon_editor/layer.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <vector>

#include "app/application.h"
#include "app/layer.h"
#include "protegon_editor/panels.h"
#include "renderer/renderer.h"
#include "renderer/resources/id.h"
#include "runtime/ecs/entity.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_manager.h"

namespace ptgn {

namespace editor {

void EditorLayer::OnUpdate() {}

void EditorLayer::OnRender() {
	hierarchy_drag_active_ = false;

	editor::DrawDockspace(*this);
	editor::DrawHierarchyWindow(*this);
	editor::DrawScenesWindow(*this);
	editor::DrawInspectorWindow(*this);
	editor::DrawEngineSettingsWindow(*this);
	editor::DrawAssetsWindow(*this);
	editor::DrawGameWindow(*this);
}

std::size_t EditorLayer::GetEntityCount() const {
	std::size_t total{ 0 };
	for (const auto& scene : app.scene_manager_.GetScenes()) {
		total += scene->GetEntityCount();
	}
	return total;
}

impl::TextureId EditorLayer::GetScreenTargetId() const {
	return app.renderer_.GetRenderTargetTexture(app.renderer_.GetScreenTarget());
}

EditorLayer::EditorLayer(Application& app) : app{ app } {}

Entity EditorLayer::FindEntityById(int id) const {
	// TODO: Fix.
	return {};
}

std::vector<int> EditorLayer::GetChildrenIds(int parent_id) const {
	std::vector<int> ids;
	// TODO: Fix.
	// for (const auto& e : CurrentScene().entities) {
	//	if (e.parent_id == parent_id) {
	//		ids.push_back(e.id);
	//	}
	//}
	return ids;
}

bool EditorLayer::IsDescendantOf(int entity_id, int possible_parent_id) const {
	Entity e = FindEntityById(entity_id);
	while (e) {
		// TODO: Fix.
		// if (e->parent_id == possible_parent_id) {
		//	return true;
		//}
		// e = FindEntityById(e->parent_id);
	}
	return false;
}

void EditorLayer::AddEntityAtRoot() {
	Entity e;
	// e.id			= next_entity_id_++;
	// e.parent_id		= -1;
	// e.name			= "New Entity";
	// e.has_transform = true;
	// CurrentScene().entities.push_back(e);
	// selected_entity_id_ = e.id;
	// selected_component_ = ComponentKind::Transform;
	//  TODO: Fix.
}

void EditorLayer::DeleteEntityRecursive(int entity_id) {
	auto children = GetChildrenIds(entity_id);
	for (int child_id : children) {
		DeleteEntityRecursive(child_id);
	}

	// TODO: Fix.
	/*
	auto& entities = CurrentScene().entities;
	entities.erase(
		std::remove_if(
			entities.begin(), entities.end(),
			[entity_id](const Entity& e) { return e.id == entity_id; }
		),
		entities.end()
	);

	if (selected_entity_id_ == entity_id) {
		selected_entity_id_ = -1;
		selected_component_ = ComponentKind::None;
	}
	*/
}

void EditorLayer::DeleteSelectedEntityAndSelectPrevious() {
	if (selected_entity_id_ < 0) {
		return;
	}

	DeleteEntityRecursive(selected_entity_id_);

	const int fallback_id = GetLastEntityInHierarchyOrder();
	// TODO: Fix.
	// selected_entity_id_	  = fallback_id;
	// selected_component_	  = (fallback_id >= 0) ? ComponentKind::Transform : ComponentKind::None;
}

void EditorLayer::MoveEntityToRoot(int entity_id) {
	if (Entity e = FindEntityById(entity_id)) {
		// TODO: Fix.
		// e->parent_id = -1;
	}
}

void EditorLayer::ReparentEntity(int entity_id, int new_parent_id) {
	if (entity_id == new_parent_id) {
		return;
	}
	if (new_parent_id >= 0 && IsDescendantOf(new_parent_id, entity_id)) {
		return;
	}
	if (Entity e = FindEntityById(entity_id)) {
		// TODO: Fix.
		/*e->parent_id		= new_parent_id;
		expand_entity_once_ = new_parent_id;*/
	}
}

// void EditorLayer::AddComponentToSelected(ComponentKind kind) {
//	Entity* e = FindEntityById(selected_entity_id_);
//	if (!e) {
//		return;
//	}
//
//	switch (kind) {
//		case ComponentKind::Transform:		e->has_transform = true; break;
//		case ComponentKind::SpriteRenderer: e->has_sprite_renderer = true; break;
//		case ComponentKind::Camera2D:		e->has_camera_2d = true; break;
//		case ComponentKind::RigidBody2D:	e->has_rigidbody_2d = true; break;
//		case ComponentKind::Script:			e->has_script = true; break;
//		default:							break;
//	}
// }

// TODO: Fix.
// void EditorLayer::RemoveComponentFromSelected(ComponentKind kind) {
//	Entity* e = FindEntityById(selected_entity_id_);
//	if (!e) {
//		return;
//	}
//
//	switch (kind) {
//		case ComponentKind::Transform:		break;
//		case ComponentKind::SpriteRenderer: e->has_sprite_renderer = false; break;
//		case ComponentKind::Camera2D:		e->has_camera_2d = false; break;
//		case ComponentKind::RigidBody2D:	e->has_rigidbody_2d = false; break;
//		case ComponentKind::Script:			e->has_script = false; break;
//		default:							break;
//	}
//
//	if (selected_component_ == kind) {
//		selected_component_ = ComponentKind::Transform;
//	}
//}

void EditorLayer::BeginRenameEntityInline(Entity entity) {
	// TODO: Fix.
	// renaming_entity_id_	  = entity.id;
	// renaming_scene_index_ = -1;
	// std::snprintf(rename_buffer_, sizeof(rename_buffer_), "%s", entity.name.c_str());
}

void EditorLayer::BeginRenameSceneInline(int scene_index) {
	// TODO: Fix.
	// renaming_scene_index_ = scene_index;
	// renaming_entity_id_	  = -1;
	// std::snprintf(rename_buffer_, sizeof(rename_buffer_), "%s",
	// scenes_[scene_index].name.c_str());
}

void EditorLayer::CommitInlineRename() {
	// TODO: Fix.
	// if (renaming_entity_id_ >= 0) {
	//	if (Entity entity = FindEntityById(renaming_entity_id_)) {
	//		entity->name = rename_buffer_;
	//	}
	// } else if (renaming_scene_index_ >= 0 &&
	//		   renaming_scene_index_ < static_cast<int>(scenes_.size())) {
	//	scenes_[renaming_scene_index_].name = rename_buffer_;
	// }

	renaming_entity_id_	  = -1;
	renaming_scene_index_ = -1;
	rename_buffer_[0]	  = '\0';
}

void EditorLayer::CancelInlineRename() {
	renaming_entity_id_	  = -1;
	renaming_scene_index_ = -1;
	rename_buffer_[0]	  = '\0';
}

void EditorLayer::DeleteScene(int scene_index) {
	// TODO: Fix.
	/*if (scene_index < 0 || scene_index >= static_cast<int>(scenes_.size())) {
		return;
	}

	scenes_.erase(scenes_.begin() + scene_index);

	if (scenes_.empty()) {
		SceneData scene;
		scene.name = "NewScene_1";
		scene.entities.push_back(
			MakeEntity(next_entity_id_++, -1, "Main Camera", true, false, true, false, false)
		);
		scenes_.push_back(scene);
		current_scene_index_ = 0;
	} else {
		if (current_scene_index_ >= static_cast<int>(scenes_.size())) {
			current_scene_index_ = static_cast<int>(scenes_.size()) - 1;
		} else if (scene_index < current_scene_index_) {
			--current_scene_index_;
		} else if (scene_index == current_scene_index_) {
			current_scene_index_ =
				(std::min)(current_scene_index_, static_cast<int>(scenes_.size()) - 1);
		}
	}

	selected_entity_id_ = CurrentScene().entities.empty() ? -1 : CurrentScene().entities.front().id;
	selected_component_ =
		(selected_entity_id_ >= 0) ? ComponentKind::Transform : ComponentKind::None;*/
}

Entity EditorLayer::MakeEntity(
	int id, int parent_id, const char* name, bool transform, bool sprite, bool camera,
	bool rigidbody, bool script
) {
	Entity e;
	// TODO: Fix.
	// e.id				  = id;
	// e.parent_id			  = parent_id;
	// e.name				  = name;
	// e.has_transform		  = transform;
	// e.has_sprite_renderer = sprite;
	// e.has_camera_2d		  = camera;
	// e.has_rigidbody_2d	  = rigidbody;
	// e.has_script		  = script;
	return e;
}

void EditorLayer::InitializeProject() {
	// TODO: Fix.

	// SceneData level_01;
	// level_01.name	  = "Level_01";
	// level_01.entities = {
	//	MakeEntity(1, -1, "Main Camera", true, false, true, false, false),
	//	MakeEntity(2, -1, "Directional Light", true, false, false, false, false),
	//	MakeEntity(3, -1, "Player", true, true, false, true, true),
	//	MakeEntity(4, 3, "Weapon", true, true, false, false, false),
	//	MakeEntity(5, 3, "Camera Pivot", true, false, false, false, false),
	// };

	// SceneData ui_scene;
	// ui_scene.name	  = "UI";
	// ui_scene.entities = {
	//	MakeEntity(101, -1, "UICamera", true, false, true, false, false),
	//	MakeEntity(102, -1, "Canvas", true, false, false, false, false),
	// };

	// scenes_.push_back(level_01);
	// scenes_.push_back(ui_scene);

	// current_scene_index_ = 0;
	// selected_entity_id_	 = 3;
	// selected_component_	 = ComponentKind::Transform;
	// next_entity_id_		 = 1000;
}

void EditorLayer::BuildDefaultDockLayout(unsigned int dockspace_id) {
	if (dock_layout_built_) {
		return;
	}

	dock_layout_built_ = true;

	ImGui::DockBuilderRemoveNode(dockspace_id);
	ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_None);

	const ImVec2 work_size = ImGui::GetMainViewport()->WorkSize;
	ImGui::DockBuilderSetNodeSize(dockspace_id, work_size);

	// Desired side widths as fractions of the full dockspace width.
	const float hierarchy_width_ratio = 0.2f;
	const float inspector_width_ratio = 0.2f;

	const float left_width	= work_size.x * hierarchy_width_ratio;
	const float right_width = work_size.x * inspector_width_ratio;

	// Remaining width belongs to the center column.
	float center_width = work_size.x - left_width - right_width;
	if (center_width < 100.0f) {
		center_width = 100.0f;
	}

	// Desired initial Game height from 16:9 based on center width.
	float desired_game_height = center_width * (9.0f / 16.0f);

	// Clamp in case the available height is too small.
	if (desired_game_height > work_size.y) {
		desired_game_height = work_size.y;
	}

	float bottom_center_height = work_size.y - desired_game_height;
	if (bottom_center_height < 0.0f) {
		bottom_center_height = 0.0f;
	}

	const float left_ratio			= left_width / work_size.x;
	const float right_ratio			= right_width / (work_size.x - left_width);
	const float center_bottom_ratio = bottom_center_height / work_size.y;

	ImGuiID dock_main  = dockspace_id;
	ImGuiID dock_left  = 0;
	ImGuiID dock_right = 0;

	ImGuiID dock_left_bottom   = 0;
	ImGuiID dock_right_bottom  = 0;
	ImGuiID dock_center_bottom = 0;

	// First create left / center / right columns.
	dock_left =
		ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left, left_ratio, nullptr, &dock_main);

	dock_right =
		ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Right, right_ratio, nullptr, &dock_main);

	// Split left column: Hierarchy on top, Scenes on bottom.
	dock_left_bottom =
		ImGui::DockBuilderSplitNode(dock_left, ImGuiDir_Down, 0.35f, nullptr, &dock_left);

	// Split right column: Inspector on top, Engine Settings on bottom.
	dock_right_bottom =
		ImGui::DockBuilderSplitNode(dock_right, ImGuiDir_Down, 0.35f, nullptr, &dock_right);

	// Split center column: Game on top, empty window on bottom.
	if (center_bottom_ratio > 0.0f) {
		dock_center_bottom = ImGui::DockBuilderSplitNode(
			dock_main, ImGuiDir_Down, center_bottom_ratio, nullptr, &dock_main
		);
	}

	ImGui::DockBuilderDockWindow("###SceneHierarchyWindow", dock_left);
	ImGui::DockBuilderDockWindow("Scenes", dock_left_bottom);

	ImGui::DockBuilderDockWindow("Inspector", dock_right);
	ImGui::DockBuilderDockWindow("Engine Settings", dock_right_bottom);

	ImGui::DockBuilderDockWindow("Game", dock_main);

	if (dock_center_bottom != 0) {
		ImGui::DockBuilderDockWindow("Assets", dock_center_bottom);
	}

	ImGui::DockBuilderFinish(dockspace_id);
}

int EditorLayer::GetLastEntityInHierarchyOrder() const {
	// TODO: Fix.
	// if (CurrentScene().entities.empty()) {
	//	return -1;
	// }

	auto roots = GetChildrenIds(-1);
	if (roots.empty()) {
		// TODO: Fix.
		// return CurrentScene().entities.back().id;
		return -1;
	}

	int last_id = -1;

	const auto visit = [&](const auto& self, int id) -> void {
		last_id		  = id;
		auto children = GetChildrenIds(id);
		for (int child_id : children) {
			self(self, child_id);
		}
	};

	for (int root_id : roots) {
		visit(visit, root_id);
	}

	return last_id;
}

std::vector<int> EditorLayer::CollectSubtreeIds(int root_entity_id) const {
	std::vector<int> result;

	const auto visit = [&](const auto& self, int entity_id) -> void {
		result.push_back(entity_id);
		for (int child_id : GetChildrenIds(entity_id)) {
			self(self, child_id);
		}
	};

	visit(visit, root_entity_id);
	return result;
}

void EditorLayer::ReparentEntityBefore(int entity_id, int new_parent_id, int before_entity_id) {
	if (entity_id == before_entity_id) {
		return;
	}

	if (new_parent_id >= 0) {
		if (entity_id == new_parent_id) {
			return;
		}
		if (IsDescendantOf(new_parent_id, entity_id)) {
			return;
		}
	}

	auto subtree_ids = CollectSubtreeIds(entity_id);
	if (subtree_ids.empty()) {
		return;
	}

	// TODO: Fix.
	/*
	auto& entities = CurrentScene().entities;

	std::vector<Entity> moving_block;
	moving_block.reserve(subtree_ids.size());

	for (int id : subtree_ids) {
		auto it = std::find_if(entities.begin(), entities.end(), [id](const Entity& e) {
			return e.id == id;
		});
		if (it != entities.end()) {
			moving_block.push_back(*it);
		}
	}

	entities.erase(
		std::remove_if(
			entities.begin(), entities.end(),
			[&](const Entity& e) {
				return std::find(subtree_ids.begin(), subtree_ids.end(), e.id) != subtree_ids.end();
			}
		),
		entities.end()
	);

	if (!moving_block.empty()) {
		moving_block.front().parent_id = new_parent_id;
	}

	auto insert_it =
		std::find_if(entities.begin(), entities.end(), [before_entity_id](const Entity& e) {
			return e.id == before_entity_id;
		});

	entities.insert(insert_it, moving_block.begin(), moving_block.end());

	if (new_parent_id >= 0) {
		expand_entity_once_ = new_parent_id;
	}
	*/
}

void EditorLayer::ReparentEntityAtEnd(int entity_id, int new_parent_id) {
	if (entity_id == new_parent_id) {
		return;
	}

	if (new_parent_id >= 0) {
		if (IsDescendantOf(new_parent_id, entity_id)) {
			return;
		}
	}

	auto subtree_ids = CollectSubtreeIds(entity_id);
	if (subtree_ids.empty()) {
		return;
	}

	// TODO: Fix.
	/*
	auto& entities = CurrentScene().entities;

	std::vector<Entity> moving_block;
	moving_block.reserve(subtree_ids.size());

	for (int id : subtree_ids) {
		auto it = std::find_if(entities.begin(), entities.end(), [id](const Entity& e) {
			return e.id == id;
		});
		if (it != entities.end()) {
			moving_block.push_back(*it);
		}
	}

	entities.erase(
		std::remove_if(
			entities.begin(), entities.end(),
			[&](const Entity& e) {
				return std::find(subtree_ids.begin(), subtree_ids.end(), e.id) != subtree_ids.end();
			}
		),
		entities.end()
	);

	if (!moving_block.empty()) {
		moving_block.front().parent_id = new_parent_id;
	}

	auto insert_it = entities.end();

	if (new_parent_id >= 0) {
		int last_descendant_id = -1;
		for (const auto& e : entities) {
			if (e.parent_id == new_parent_id || IsDescendantOf(e.id, new_parent_id)) {
				last_descendant_id = e.id;
			}
		}

		if (last_descendant_id >= 0) {
			insert_it = std::find_if(
				entities.begin(), entities.end(),
				[last_descendant_id](const Entity& e) { return e.id == last_descendant_id; }
			);
			if (insert_it != entities.end()) {
				++insert_it;
			}
		} else {
			auto parent_it =
				std::find_if(entities.begin(), entities.end(), [new_parent_id](const Entity& e) {
					return e.id == new_parent_id;
				});
			if (parent_it != entities.end()) {
				insert_it = parent_it + 1;
			}
		}

		expand_entity_once_ = new_parent_id;
	}

	entities.insert(insert_it, moving_block.begin(), moving_block.end());
	*/
}

} // namespace editor

} // namespace ptgn