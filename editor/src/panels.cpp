#include "protegon_editor/panels.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <cctype>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "app/application.h"
#include "protegon_editor/layer.h"
#include "runtime/ecs/entity.h"

namespace ptgn {

namespace editor {

static void DrawAddComponentMenuItems(Application& app) {
	// TODO: Fix.
	/*

	Entity* entity = app.FindEntityById(app.selected_entity_id_);
	if (!entity) {
		return;
	}

	if (!entity->has_sprite_renderer && ImGui::MenuItem("SpriteRenderer")) {
		app.AddComponentToSelected(ComponentKind::SpriteRenderer);
	}
	if (!entity->has_camera_2d && ImGui::MenuItem("Camera2D")) {
		app.AddComponentToSelected(ComponentKind::Camera2D);
	}
	if (!entity->has_rigidbody_2d && ImGui::MenuItem("RigidBody2D")) {
		app.AddComponentToSelected(ComponentKind::RigidBody2D);
	}
	if (!entity->has_script && ImGui::MenuItem("Script")) {
		app.AddComponentToSelected(ComponentKind::Script);
	}
	*/
}

static bool DrawInlineRenameField(Application& app, int id, bool is_entity) {
	return false;
	// TODO: Fix.
	/*
	ImGui::PushID(id);

	const bool first_frame_of_rename = (is_entity && app.renaming_entity_id_ == id) ||
									   (!is_entity && app.renaming_scene_index_ >= 0 &&
										(id - 100000) == app.renaming_scene_index_);

	if (first_frame_of_rename) {
		ImGui::SetKeyboardFocusHere();
	}

	const bool submitted = ImGui::InputText(
		"##RenameInline", app.rename_buffer_, sizeof(app.rename_buffer_),
		ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll
	);

	const bool lost_focus = ImGui::IsItemDeactivated();

	if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
		app.CancelInlineRename();
		ImGui::PopID();
		return true;
	}

	if (submitted || lost_focus) {
		app.CommitInlineRename();
		ImGui::PopID();
		return true;
	}

	ImGui::PopID();
	return false;
	*/
}

static bool EntityMatchesFilter(const char* entity_name, const char* filter_text) {
	if (!filter_text || filter_text[0] == '\0') {
		return true;
	}

	auto trim = [](std::string s) {
		const auto first = s.find_first_not_of(" \t");
		if (first == std::string::npos) {
			return std::string{};
		}
		const auto last = s.find_last_not_of(" \t");
		return s.substr(first, last - first + 1);
	};

	auto lower = [](std::string s) {
		for (char& c : s) {
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		}
		return s;
	};

	const std::string name	 = lower(entity_name ? std::string(entity_name) : std::string{});
	const std::string filter = filter_text ? std::string(filter_text) : std::string{};

	bool has_include	 = false;
	bool matched_include = false;

	size_t start = 0;
	while (start <= filter.size()) {
		const size_t comma = filter.find(',', start);
		std::string token  = (comma == std::string::npos) ? filter.substr(start)
														  : filter.substr(start, comma - start);

		token = lower(trim(token));

		if (!token.empty()) {
			const bool exclude		 = token[0] == '-';
			const std::string needle = trim(exclude ? token.substr(1) : token);

			if (!needle.empty()) {
				const bool contains = (name.find(needle) != std::string::npos);

				if (exclude) {
					if (contains) {
						return false;
					}
				} else {
					has_include = true;
					if (contains) {
						matched_include = true;
					}
				}
			}
		}

		if (comma == std::string::npos) {
			break;
		}
		start = comma + 1;
	}

	return !has_include || matched_include;
}

static bool EntityOrDescendantMatchesFilter(Application& app, int entity_id) {
	return false;
	// TODO: Fix.
	/*
	const Entity* entity = app.FindEntityById(entity_id);
	if (!entity) {
		return false;
	}

	if (EntityMatchesFilter(entity->name.c_str(), app.hierarchy_filter_)) {
		return true;
	}

	const auto children = app.GetChildrenIds(entity_id);
	for (int child_id : children) {
		if (EntityOrDescendantMatchesFilter(app, child_id)) {
			return true;
		}
	}

	return false;
	*/
}

static bool IsHierarchyEntityDragActive() {
	const ImGuiPayload* payload = ImGui::GetDragDropPayload();
	return payload && payload->IsDataType("PTGN_ENTITY");
}

static int GetDraggedEntityId() {
	const ImGuiPayload* payload = ImGui::GetDragDropPayload();
	if (!payload || !payload->IsDataType("PTGN_ENTITY") || payload->DataSize != sizeof(int)) {
		return -1;
	}
	return *static_cast<const int*>(payload->Data);
}

static std::vector<int> GetVisibleSiblingIdsForParent(Application& app, int parent_id) {
	std::vector<int> result;
	// TODO: Fix.
	// for (const auto& e : app.CurrentScene().entities) {
	//	if (e.parent_id == parent_id && EntityOrDescendantMatchesFilter(app, e.id)) {
	//		result.push_back(e.id);
	//	}
	//}
	return result;
}

static bool IsFirstVisibleSibling(Application& app, int entity_id, int parent_id) {
	const auto siblings = GetVisibleSiblingIdsForParent(app, parent_id);
	return !siblings.empty() && siblings.front() == entity_id;
}

static bool IsLastVisibleSibling(Application& app, int entity_id, int parent_id) {
	const auto siblings = GetVisibleSiblingIdsForParent(app, parent_id);
	return !siblings.empty() && siblings.back() == entity_id;
}

static int GetVisibleSiblingIndex(Application& app, int entity_id, int parent_id) {
	const auto siblings = GetVisibleSiblingIdsForParent(app, parent_id);
	for (int i = 0; i < static_cast<int>(siblings.size()); ++i) {
		if (siblings[i] == entity_id) {
			return i;
		}
	}
	return -1;
}

static bool IsImmediateNextVisibleSibling(
	Application& app, int entity_id, int before_entity_id, int parent_id
) {
	const auto siblings = GetVisibleSiblingIdsForParent(app, parent_id);
	for (int i = 0; i + 1 < static_cast<int>(siblings.size()); ++i) {
		if (siblings[i] == entity_id && siblings[i + 1] == before_entity_id) {
			return true;
		}
	}
	return false;
}

static void DrawRootSectionHeader(const char* label) {
	ImGui::Spacing();
	ImGui::TextUnformatted(label);
	ImGui::Separator();
}

static void BeginLabeledRow(const char* label, float label_width = 90.0f) {
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(label);
	ImGui::SameLine();
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (label_width - ImGui::CalcTextSize(label).x));
	ImGui::SameLine();
	ImGui::SetNextItemWidth(-1.0f);
}

static void DrawSiblingEndDropDivider(Application& app, const char* id_suffix, int parent_id) {
	if (!IsHierarchyEntityDragActive()) {
		return;
	}

	const int dragged_id = GetDraggedEntityId();
	if (dragged_id >= 0) {
		if (parent_id == dragged_id) {
			return;
		}
		// TODO: Fix.
		/*if (parent_id >= 0 && app.IsDescendantOf(parent_id, dragged_id)) {
			return;
		}*/

		// Hide the final divider of the dragged entity's own sibling generation
		// when dragging the last sibling.
		if (IsLastVisibleSibling(app, dragged_id, parent_id)) {
			return;
		}
	}

	ImGui::PushID(id_suffix);

	const float width  = ImGui::GetContentRegionAvail().x;
	const float height = 4.0f;

	ImGui::InvisibleButton("SiblingEndDropDivider", ImVec2(width, height));

	if (ImGui::BeginDragDropTarget()) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PTGN_ENTITY")) {
			const int dropped_id = *static_cast<const int*>(payload->Data);

			// TODO: Fix.
			/*if (dropped_id != parent_id && !app.IsDescendantOf(parent_id, dropped_id)) {
				app.ReparentEntityAtEnd(dropped_id, parent_id);
			}*/
		}
		ImGui::EndDragDropTarget();
	}

	ImGui::PopID();
}

static void DrawSiblingDropDivider(
	Application& app, const char* id_suffix, int parent_id, int before_entity_id
) {
	if (!IsHierarchyEntityDragActive()) {
		return;
	}

	const int dragged_id = GetDraggedEntityId();
	if (dragged_id >= 0) {
		// TODO: Fix.
		/*if (before_entity_id == dragged_id || app.IsDescendantOf(before_entity_id, dragged_id)) {
			return;
		}*/
		if (parent_id >= 0 && parent_id == dragged_id) {
			return;
		}

		// Hide the first divider in the dragged entity's own sibling generation
		// when dragging the first sibling.
		if (IsFirstVisibleSibling(app, dragged_id, parent_id) &&
			IsFirstVisibleSibling(app, before_entity_id, parent_id)) {
			return;
		}

		// Hide the divider directly below the dragged entity in its own generation.
		// That divider is the one immediately before the next sibling.
		if (IsImmediateNextVisibleSibling(app, dragged_id, before_entity_id, parent_id)) {
			return;
		}
	}

	ImGui::PushID(id_suffix);

	const float width  = ImGui::GetContentRegionAvail().x;
	const float height = 3.0f;

	ImGui::InvisibleButton("SiblingDropDivider", ImVec2(width, height));

	if (ImGui::BeginDragDropTarget()) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PTGN_ENTITY")) {
			const int dropped_id = *static_cast<const int*>(payload->Data);

			// TODO: Fix.
			/*if (dropped_id != before_entity_id && dropped_id != parent_id &&
				!app.IsDescendantOf(parent_id, dropped_id)) {
				app.ReparentEntityBefore(dropped_id, parent_id, before_entity_id);
			}*/
		}
		ImGui::EndDragDropTarget();
	}

	ImGui::PopID();
}

static void DrawEntityNodeRecursive(
	Application& app, int entity_id, int visible_sibling_index, int visible_sibling_count
) {
	// TODO: Fix.
	/*
	Entity* entity = app.FindEntityById(entity_id);
	if (!entity) {
		return;
	}

	if (!EntityOrDescendantMatchesFilter(app, entity_id)) {
		return;
	}

	auto all_children = app.GetChildrenIds(entity_id);
	std::vector<int> visible_children;
	visible_children.reserve(all_children.size());

	for (int child_id : all_children) {
		if (EntityOrDescendantMatchesFilter(app, child_id)) {
			visible_children.push_back(child_id);
		}
	}

	DrawSiblingDropDivider(
		app, (std::string("BeforeEntity_") + std::to_string(entity_id)).c_str(), entity->parent_id,
		entity_id
	);

	if (app.expand_entity_once_ == entity_id) {
		ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		app.expand_entity_once_ = -1;
	}

	const bool renaming_this_entity = (app.renaming_entity_id_ == entity_id);

	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
							   ImGuiTreeNodeFlags_OpenOnDoubleClick |
							   ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;

	if (visible_children.empty()) {
		flags |= ImGuiTreeNodeFlags_Leaf;
	}

	if (app.selected_entity_id_ == entity_id && !renaming_this_entity) {
		flags |= ImGuiTreeNodeFlags_Selected;
	}

	if (renaming_this_entity) {
		flags |= ImGuiTreeNodeFlags_AllowOverlap;
	}

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 1.0f));

	bool open = false;

	if (renaming_this_entity) {
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));

		open =
			ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(entity_id)), flags, "");

		ImGui::PopStyleColor(3);

		if (!open) {
			ImGui::PopStyleVar();
			app.CancelInlineRename();
			return;
		}

		ImGui::SameLine(0.0f, 0.0f);
		ImGui::SetNextItemWidth(-1.0f);
		DrawInlineRenameField(app, entity_id, true);
	} else {
		open = ImGui::TreeNodeEx(
			reinterpret_cast<void*>(static_cast<intptr_t>(entity_id)), flags, "%s",
			entity->name.c_str()
		);

		if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
			app.selected_entity_id_ = entity_id;
			app.selected_component_ =
				entity->has_transform ? ComponentKind::Transform : ComponentKind::None;
		}

		if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
			app.selected_entity_id_ = entity_id;
			app.selected_component_ =
				entity->has_transform ? ComponentKind::Transform : ComponentKind::None;
		}

		if (ImGui::BeginDragDropSource()) {
			ImGui::SetDragDropPayload("PTGN_ENTITY", &entity_id, sizeof(entity_id));
			ImGui::Text("Move %s", entity->name.c_str());
			ImGui::EndDragDropSource();
		}

		const int dragged_id = GetDraggedEntityId();
		const bool suppress_entity_drop_target =
			(dragged_id >= 0) &&
			(entity_id == dragged_id || app.IsDescendantOf(entity_id, dragged_id));

		if (!suppress_entity_drop_target && ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PTGN_ENTITY")) {
				const int dropped_id = *static_cast<const int*>(payload->Data);
				if (dropped_id != entity_id && !app.IsDescendantOf(entity_id, dropped_id)) {
					app.ReparentEntity(dropped_id, entity_id);
				}
			}
			ImGui::EndDragDropTarget();
		}

		if (ImGui::BeginPopupContextItem()) {
			app.selected_entity_id_ = entity_id;
			app.selected_component_ =
				entity->has_transform ? ComponentKind::Transform : ComponentKind::None;

			if (ImGui::MenuItem("Rename")) {
				app.BeginRenameEntityInline(*entity);
			}
			if (ImGui::BeginMenu("Add Component")) {
				DrawAddComponentMenuItems(app);
				ImGui::EndMenu();
			}
			if (ImGui::MenuItem("Create Child")) {
				Entity child;
				child.id			= app.next_entity_id_++;
				child.parent_id		= entity_id;
				child.name			= "New Child";
				child.has_transform = true;
				app.CurrentScene().entities.push_back(child);
				app.expand_entity_once_ = entity_id;
			}
			if (ImGui::MenuItem("Move To Root")) {
				app.MoveEntityToRoot(entity_id);
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Delete")) {
				app.selected_entity_id_ = entity_id;
				app.DeleteSelectedEntityAndSelectPrevious();
				ImGui::EndPopup();
				if (open) {
					ImGui::TreePop();
				}
				ImGui::PopStyleVar();
				return;
			}
			ImGui::EndPopup();
		}
	}

	ImGui::PopStyleVar();

	if (open) {
		for (std::size_t i = 0; i < visible_children.size(); ++i) {
			DrawEntityNodeRecursive(
				app, visible_children[i], static_cast<int>(i),
				static_cast<int>(visible_children.size())
			);
		}

		bool should_show_end_children_divider = true;

		const int dragged_id = GetDraggedEntityId();
		if (dragged_id >= 0) {
			const Entity* dragged_entity = app.FindEntityById(dragged_id);

			// If the dragged entity is coming from outside this child list,
			// dropping on the parent row already places it as the last child,
			// so the trailing divider is redundant.
			if (dragged_entity && dragged_entity->parent_id != entity_id) {
				should_show_end_children_divider = false;
			}
		}

		if (should_show_end_children_divider) {
			DrawSiblingEndDropDivider(
				app, (std::string("EndChildren_") + std::to_string(entity_id)).c_str(), entity_id
			);
		}

		ImGui::TreePop();
	}

	*/
}

bool DrawComponentHeader(Application& app, const char* label, int id, bool non_removable) {
	ImGui::PushID(id);

	const char* popup_id = "ComponentContextMenu";
	bool popup_open		 = ImGui::IsPopupOpen(popup_id, ImGuiPopupFlags_None);

	if (popup_open) {
		ImVec4 active = ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive);
		ImGui::PushStyleColor(ImGuiCol_Header, active);
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, active);
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, active);
	}

	bool open = ImGui::CollapsingHeader(label);

	if (popup_open) {
		ImGui::PopStyleColor(3);
	}

	if (!non_removable) {
		if (ImGui::BeginPopupContextItem(popup_id)) {
			if (ImGui::MenuItem("Remove Component")) {
				// TODO: Fix.
				// app.RemoveComponentFromSelected(kind);
			}
			ImGui::EndPopup();
		}
	}

	ImGui::PopID();
	return open;
}

void DrawDockspace(EditorLayer& layer, Application& app) {
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
							 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
							 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
							 ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_MenuBar;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	ImGui::Begin("DockspaceRoot", nullptr, flags);
	ImGui::PopStyleVar(3);

	if (ImGui::BeginMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			ImGui::MenuItem("New Scene", "Ctrl+N");
			ImGui::MenuItem("Open Scene...", "Ctrl+O");
			ImGui::Separator();
			ImGui::MenuItem("Save", "Ctrl+S");
			ImGui::MenuItem("Save As...", "Ctrl+Shift+S");
			ImGui::Separator();
			ImGui::MenuItem("Exit");
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	ImGuiID dockspace_id = ImGui::GetID("MainDockspace");
	layer.BuildDefaultDockLayout(dockspace_id);
	ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

	ImGui::End();
}

void DrawHierarchyWindow(EditorLayer& layer, Application& app) {
	// TODO: Fix.
	std::string window_name = "Test Scene1" + std::string("###SceneHierarchyWindow");
	// std::string window_name = app.CurrentScene().name + "###SceneHierarchyWindow";
	ImGui::Begin(window_name.c_str());

	if (ImGui::BeginTabBar("SceneTabs")) {
		if (ImGui::BeginTabItem("Hierarchy")) {
			BeginLabeledRow("Name");
			char scene_name_buffer[128];
			std::snprintf(scene_name_buffer, sizeof(scene_name_buffer), "%s", window_name.c_str());
			if (ImGui::InputText("##SceneName", scene_name_buffer, sizeof(scene_name_buffer))) {
				window_name = scene_name_buffer;
			}

			BeginLabeledRow("Entity Actions");
			const float avail	 = ImGui::GetContentRegionAvail().x;
			const float gap		 = ImGui::GetStyle().ItemSpacing.x;
			const float button_w = (avail - gap) * 0.5f;

			if (ImGui::Button("Add", ImVec2(button_w, 0.0f))) {
				layer.AddEntityAtRoot();
			}
			ImGui::SameLine();
			if (ImGui::Button("Delete", ImVec2(button_w, 0.0f))) {
				layer.DeleteSelectedEntityAndSelectPrevious();
			}

			BeginLabeledRow("Entity Filter");
			ImGui::InputTextWithHint(
				"##HierarchyFilter", "incl, -excl", layer.hierarchy_filter_,
				sizeof(layer.hierarchy_filter_)
			);

			DrawRootSectionHeader("Entity List");

			std::vector<int> visible_root_ids;
			// TODO: Fix.
			/*for (const auto& e : layer.CurrentScene().entities) {
				if (e.parent_id == -1 && EntityOrDescendantMatchesFilter(app, e.id)) {
					visible_root_ids.push_back(e.id);
				}
			}*/

			for (std::size_t i = 0; i < visible_root_ids.size(); ++i) {
				DrawEntityNodeRecursive(
					app, visible_root_ids[i], static_cast<int>(i),
					static_cast<int>(visible_root_ids.size())
				);
			}

			DrawSiblingEndDropDivider(app, "EndRootDropTarget", -1);
			ImGui::Separator();

			if (ImGui::BeginPopupContextWindow(
					"HierarchyContext",
					ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems
				)) {
				if (ImGui::MenuItem("Add Root Entity")) {
					layer.AddEntityAtRoot();
				}
				ImGui::EndPopup();
			}

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Settings")) {
			BeginLabeledRow("Clear Color");
			ImGui::ColorEdit4("##ClearColor", layer.clear_color_);

			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	ImGui::End();
}

void DrawScenesWindow(EditorLayer& layer, Application& app) {
	ImGui::Begin("Scenes");

	// TODO: Fix.
	for (int i = 0; i < static_cast<int>(1); ++i) {
		if (layer.renaming_scene_index_ == i) {
			DrawInlineRenameField(app, i + 100000, false);
			continue;
		}

		const bool selected = (layer.current_scene_index_ == i);

		// TODO: Fix.
		std::string scene_name = "Test";
		if (ImGui::Selectable(scene_name.c_str(), selected)) {
			// TODO: Fix.
			/*layer.current_scene_index_ = i;
			layer.selected_entity_id_ =
				layer.CurrentScene().entities.empty() ? -1 :
			layer.CurrentScene().entities.front().id; layer.selected_component_ =
			ComponentKind::Transform;*/
		}

		if (ImGui::BeginPopupContextItem()) {
			if (ImGui::MenuItem("Rename")) {
				layer.BeginRenameSceneInline(i);
			}
			if (ImGui::MenuItem("Delete")) {
				layer.DeleteScene(i);
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
			scene.name = "NewScene_" + std::to_string(static_cast<int>(layer.scenes_.size()) + 1);
			scene.entities.push_back(layer.MakeEntity(
				layer.next_entity_id_++, -1, "Main Camera", true, false, true, false, false
			));
			layer.scenes_.push_back(scene);*/
		}
		ImGui::EndPopup();
	}

	ImGui::End();
}

namespace InspectorDetail {

// Primary template: no matching DrawComponentImpl found.
template <typename T, typename = void>
struct HasDrawComponentImpl : std::false_type {};

// Specialization: matching DrawComponentImpl exists.
template <typename T>
struct HasDrawComponentImpl<
	T, std::void_t<decltype(DrawComponentImpl(
		   std::declval<EditorLayer&>(), std::declval<Application&>(), std::declval<T&>()
	   ))>> : std::true_type {};

template <typename T>
inline constexpr bool HasDrawComponentImplV = HasDrawComponentImpl<T>::value;

} // namespace InspectorDetail

static void DrawComponentImpl(EditorLayer& layer, Application& app, Transform& transform) {
	(void)app;

	constexpr float kLabelWidth	 = 70.0f;
	constexpr float kSpacing	 = 6.0f;
	constexpr float kMinScaleAbs = 0.001f;

	auto ClampScaleAwayFromZero = [](float& value) {
		constexpr float kMin = 0.001f;

		if (value == 0.0f) {
			value = kMin; // default to positive side
		} else if (value > 0.0f && value < kMin) {
			value = kMin;
		} else if (value < 0.0f && value > -kMin) {
			value = -kMin;
		}
	};

	auto DrawCenteredLabel = [](const char* text, float width) {
		float text_width = ImGui::CalcTextSize(text).x;
		float cursor_x	 = ImGui::GetCursorPosX();
		float offset	 = (width - text_width) * 0.5f;
		if (offset > 0.0f) {
			ImGui::SetCursorPosX(cursor_x + offset);
		}
		ImGui::TextUnformatted(text);
	};

	// Position
	{
		float available_width = ImGui::GetContentRegionAvail().x;
		float right_width	  = available_width - kLabelWidth;
		float field_width	  = (right_width - 2.0f * kSpacing) / 3.0f;
		float sublabel_height = ImGui::GetTextLineHeight();

		ImGui::BeginGroup();
		ImGui::Dummy(ImVec2(0.0f, sublabel_height));
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Position");
		ImGui::EndGroup();

		ImGui::SameLine(kLabelWidth);

		ImGui::BeginGroup();

		ImGui::BeginGroup();
		DrawCenteredLabel("X", field_width);
		ImGui::SetNextItemWidth(field_width);
		float x{ transform.GetPosition().x };
		ImGui::DragFloat("##PositionX", &x, 0.1f, 0.0f, 0.0f, "%.0f");
		transform.SetPositionX(x);
		ImGui::EndGroup();

		ImGui::SameLine(0.0f, kSpacing);

		ImGui::BeginGroup();
		DrawCenteredLabel("Y", field_width);
		ImGui::SetNextItemWidth(field_width);
		float y{ transform.GetPosition().y };
		ImGui::DragFloat("##PositionY", &y, 0.1f, 0.0f, 0.0f, "%.0f");
		transform.SetPositionY(y);
		ImGui::EndGroup();

		ImGui::SameLine(0.0f, kSpacing);

		ImGui::BeginGroup();
		DrawCenteredLabel("Depth", field_width);
		ImGui::SetNextItemWidth(field_width);
		float depth{ 0.0f };
		ImGui::DragFloat("##PositionDepth", &depth, 0.05f, -1000.0f, 1000.0f, "%.0f");
		// TODO: Set depth.
		ImGui::EndGroup();

		ImGui::EndGroup();
	}

	ImGui::Spacing();

	// Scale
	{
		float available_width = ImGui::GetContentRegionAvail().x;
		float right_width	  = available_width - kLabelWidth;
		float field_width	  = (right_width - kSpacing) / 2.0f;
		float sublabel_height = ImGui::GetTextLineHeight();

		ImGui::BeginGroup();
		ImGui::Dummy(ImVec2(0.0f, sublabel_height));
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Scale");
		ImGui::EndGroup();

		ImGui::SameLine(kLabelWidth);

		ImGui::BeginGroup();

		ImGui::BeginGroup();
		DrawCenteredLabel("X", field_width);
		ImGui::SetNextItemWidth(field_width);
		float scale_x{ transform.GetScale().x };
		if (ImGui::DragFloat(
				"##ScaleX", &scale_x, 0.01f, -1000.0f, 1000.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp
			)) {
			ClampScaleAwayFromZero(scale_x);
			transform.SetScaleX(scale_x);
		}
		ImGui::EndGroup();

		ImGui::SameLine(0.0f, kSpacing);

		ImGui::BeginGroup();
		DrawCenteredLabel("Y", field_width);
		ImGui::SetNextItemWidth(field_width);
		float scale_y{ transform.GetScale().y };
		if (ImGui::DragFloat(
				"##ScaleY", &scale_y, 0.01f, -1000.0f, 1000.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp
			)) {
			ClampScaleAwayFromZero(scale_y);
			transform.SetScaleY(scale_y);
		}
		ImGui::EndGroup();

		ImGui::EndGroup();
	}

	ImGui::Spacing();

	// Rotation (now below Scale)
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted("Rotation");
	ImGui::SameLine(kLabelWidth);

	{
		float available_width = ImGui::GetContentRegionAvail().x;
		ImGui::SetNextItemWidth(available_width - kLabelWidth);
		float rotation{ transform.GetRotation().value };
		ImGui::DragFloat(
			"##Rotation", &rotation, 0.1f, 0.0f, 360.0f, "%.1f deg", ImGuiSliderFlags_AlwaysClamp
		);
		transform.SetRotation(rotation);
	}
}

// TODO: Fix.
/*
static void DrawComponentImpl(EditorLayer& layer, Application& app, SpriteRendererComponent&
sprite_renderer) { ImGui::Checkbox("Enabled##SpriteRenderer", &sprite_renderer.enabled);
	ImGui::ColorEdit4("Color", sprite_renderer.color);
	ImGui::SliderInt("Order In Layer", &sprite_renderer.order_in_layer, -20, 20);
}

static void DrawComponentImpl(EditorLayer& layer, Application& app, Camera2DComponent& camera_2d) {
	ImGui::Checkbox("Enabled##Camera2D", &camera_2d.enabled);
	ImGui::Checkbox("Primary", &camera_2d.primary);
	ImGui::SliderFloat("Size", &camera_2d.size, 1.0f, 20.0f);
}

static void DrawComponentImpl(EditorLayer& layer,Application& app, RigidBody2DComponent&
rigidbody_2d) { ImGui::Checkbox("Enabled##RigidBody2D", &rigidbody_2d.enabled);
	ImGui::SliderFloat("Mass", &rigidbody_2d.mass, 0.1f, 20.0f);
	ImGui::SliderFloat("Gravity Scale", &rigidbody_2d.gravity_scale, 0.0f, 5.0f);
	ImGui::Checkbox("Kinematic", &rigidbody_2d.is_kinematic);
}

static void DrawComponentImpl(EditorLayer& layer,Application& app, ScriptComponent& script) {
	ImGui::Checkbox("Enabled##Script", &script.enabled);
	ImGui::InputText("Class", script.class_name, sizeof(script.class_name));
}
*/

void DrawInspectorWindow(EditorLayer& layer, Application& app) {
	ImGui::Begin("Inspector");

	// TODO: Fix.
	// Entity* entity = app.FindEntityById(app.selected_entity_id_);
	// if (!entity) {
	//	ImGui::TextUnformatted("No entity selected.");
	//	ImGui::End();
	//	return;
	//}

	char name_buffer[128];
	// TODO: Fix.
	// std::snprintf(name_buffer, sizeof(name_buffer), "%s", entity->name.c_str());
	if (ImGui::InputText("Name", name_buffer, sizeof(name_buffer))) {
		// TODO: Fix.
		// entity->name = name_buffer;
	}

	ImGui::Separator();

	ImGui::Spacing();
	Transform test{ { 1, 2 }, 90.0f, { 2.0f, 2.0f } };
	DrawComponentImpl(layer, app, test);
	ImGui::Spacing();

	ImGui::Separator();
	if (ImGui::Button("Add Component", ImVec2(-1.0f, 0.0f))) {
		ImGui::OpenPopup("AddComponentPopupButton");
	}
	if (ImGui::BeginPopup("AddComponentPopupButton")) {
		DrawAddComponentMenuItems(app);
		ImGui::EndPopup();
	}

	ImGui::End();
}

struct ResolutionPreset {
	const char* label;
	int width;
	int height;
};

static constexpr ResolutionPreset kResolutionPresets[] = {
	{ "320 x 180 (16:9)", 320, 180 },		 { "640 x 360 (16:9)", 640, 360 },
	{ "800 x 450 (16:9)", 800, 450 },		 { "960 x 540 (16:9)", 960, 540 },
	{ "1280 x 720 (HD)", 1280, 720 },		 { "1600 x 900", 1600, 900 },
	{ "1920 x 1080 (Full HD)", 1920, 1080 }, { "256 x 224 (SNES)", 256, 224 },
	{ "320 x 240 (4:3)", 320, 240 },		 { "640 x 480 (VGA)", 640, 480 },
	{ "800 x 600 (SVGA)", 800, 600 },		 { "1024 x 768 (XGA)", 1024, 768 },
};

static int FindMatchingResolutionPreset(int width, int height) {
	for (int i = 0; i < IM_ARRAYSIZE(kResolutionPresets); ++i) {
		if (kResolutionPresets[i].width == width && kResolutionPresets[i].height == height) {
			return i;
		}
	}
	return -1;
}

void DrawEngineSettingsWindow(EditorLayer& layer, Application& app) {
	ImGui::Begin("Engine Settings");

	ImGui::Checkbox("Use Logical Resolution", &layer.presentation_.use_logical_resolution);

	if (layer.presentation_.use_logical_resolution) {
		if (layer.presentation_.logical_width < 1) {
			layer.presentation_.logical_width = 320;
		}
		if (layer.presentation_.logical_height < 1) {
			layer.presentation_.logical_height = 180;
		}

		constexpr float kLabelWidth = 140.0f;
		constexpr float kSpacing	= 6.0f;

		if (ImGui::BeginTable("##EngineSettingsTable", 2, ImGuiTableFlags_SizingFixedFit)) {
			ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, kLabelWidth);
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

			// --- Logical Resolution ---
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("Logical Resolution");

			ImGui::TableSetColumnIndex(1);

			int preset_index = FindMatchingResolutionPreset(
				layer.presentation_.logical_width, layer.presentation_.logical_height
			);

			const char* preview =
				(preset_index >= 0) ? kResolutionPresets[preset_index].label : "Custom";

			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::BeginCombo("##LogicalResolutionPreset", preview)) {
				for (int i = 0; i < IM_ARRAYSIZE(kResolutionPresets); ++i) {
					const bool selected = (i == preset_index);
					if (ImGui::Selectable(kResolutionPresets[i].label, selected)) {
						layer.presentation_.logical_width  = kResolutionPresets[i].width;
						layer.presentation_.logical_height = kResolutionPresets[i].height;
					}
					if (selected) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			// spacing between dropdown and fields
			ImGui::Spacing();

			float total_width = ImGui::GetContentRegionAvail().x;
			float field_width = (total_width - 6.0f) * 0.5f;

			ImGui::SetNextItemWidth(field_width);
			ImGui::DragInt(
				"##LogicalWidth", &layer.presentation_.logical_width, 1.0f, 1, 4096, "W: %d",
				ImGuiSliderFlags_AlwaysClamp
			);

			ImGui::SameLine(0.0f, 6.0f);

			ImGui::SetNextItemWidth(field_width);
			ImGui::DragInt(
				"##LogicalHeight", &layer.presentation_.logical_height, 1.0f, 1, 2160, "H: %d",
				ImGuiSliderFlags_AlwaysClamp
			);

			// --- Scaling Mode ---
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("Scaling Mode");

			ImGui::TableSetColumnIndex(1);

			const char* scaling_mode_names[] = {
				"Letterbox",
				"Stretch",
			};

			int scaling_mode = static_cast<int>(layer.presentation_.scaling_mode);

			// full width = matches both drag ints combined
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::Combo(
					"##ScalingMode", &scaling_mode, scaling_mode_names,
					IM_ARRAYSIZE(scaling_mode_names)
				)) {
				layer.presentation_.scaling_mode =
					static_cast<EditorLayer::ScalingMode>(scaling_mode);
			}

			ImGui::EndTable();
		}
	}

	ImGui::End();
}

void DrawGameWindow(EditorLayer& layer, Application& app) {
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin(
		"Game", nullptr,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
	);

	if (ImGuiWindow* game_window = ImGui::FindWindowByName("Game")) {
		if (game_window->DockNode) {
			game_window->DockNode->LocalFlags |= ImGuiDockNodeFlags_HiddenTabBar;
		}
	}

	const ImVec2 avail		= ImGui::GetContentRegionAvail();
	const ImVec2 region_min = ImGui::GetCursorScreenPos();
	const ImVec2 region_max(region_min.x + avail.x, region_min.y + avail.y);

	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	draw_list->AddRectFilled(region_min, region_max, IM_COL32(0, 0, 0, 255));

	float view_w = avail.x;
	float view_h = avail.y;

	if (layer.presentation_.use_logical_resolution) {
		const float logical_w = static_cast<float>(layer.presentation_.logical_width);
		const float logical_h = static_cast<float>(layer.presentation_.logical_height);

		if (layer.presentation_.scaling_mode == EditorLayer::ScalingMode::Letterbox) {
			const float logical_aspect = logical_w / logical_h;
			const float avail_aspect   = (avail.y > 0.0f) ? (avail.x / avail.y) : 1.0f;

			if (avail_aspect > logical_aspect) {
				view_h = avail.y;
				view_w = view_h * logical_aspect;
			} else {
				view_w = avail.x;
				view_h = view_w / logical_aspect;
			}
		} else if (layer.presentation_.scaling_mode == EditorLayer::ScalingMode::Stretch) {
			view_w = avail.x;
			view_h = avail.y;
		}
	}

	const float offset_x = (avail.x - view_w) * 0.5f;
	const float offset_y = (avail.y - view_h) * 0.5f;

	const ImVec2 view_min(region_min.x + offset_x, region_min.y + offset_y);
	const ImVec2 view_max(view_min.x + view_w, view_min.y + view_h);

	// Get your GL texture id from the renderer/screen target.
	auto gl_tex = app.GetScreenTargetId();

	//// FBO textures usually need flipped UVs in ImGui.
	draw_list->AddImage(
		(void*)(intptr_t)gl_tex, view_min, view_max, ImVec2(0.0f, 1.0f), // uv0
		ImVec2(1.0f, 0.0f)												 // uv1
	);

	draw_list->AddRect(view_min, view_max, IM_COL32(255, 255, 255, 35));

	ImGui::InvisibleButton("GameSurface", avail);

	ImGui::End();
	ImGui::PopStyleVar();
}

struct FakeAssetInfo {
	std::string key;
	std::string path;
	bool is_image		  = false;
	ImTextureID thumbnail = 0;
};

struct FakeSceneAssetRegistry {
	std::unordered_map<std::string, FakeAssetInfo> assets_by_key;
};

struct FakeAssetManager {
	std::unordered_map<std::string, FakeSceneAssetRegistry> scene_assets;

	FakeSceneAssetRegistry& GetOrCreateSceneRegistry(const std::string& scene_key) {
		return scene_assets[scene_key];
	}

	static bool IsImageFile(const std::string& path) {
		std::string ext = std::filesystem::path(path).extension().string();
		for (char& c : ext) {
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		}
		return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga" ||
			   ext == ".gif" || ext == ".webp";
	}

	void AddAssetToScene(
		const std::string& scene_key, const std::string& asset_path, ImTextureID thumbnail = 0
	) {
		FakeSceneAssetRegistry& registry = GetOrCreateSceneRegistry(scene_key);

		std::string base_key = std::filesystem::path(asset_path).stem().string();
		if (base_key.empty()) {
			base_key = "asset";
		}

		std::string key = base_key;
		int suffix		= 1;
		while (registry.assets_by_key.find(key) != registry.assets_by_key.end()) {
			key = base_key + "_" + std::to_string(suffix++);
		}

		registry.assets_by_key[key] = FakeAssetInfo{
			key,
			asset_path,
			IsImageFile(asset_path),
			thumbnail,
		};
	}

	void RemoveAssetFromScene(const std::string& scene_key, const std::string& asset_key) {
		auto scene_it = scene_assets.find(scene_key);
		if (scene_it == scene_assets.end()) {
			return;
		}

		auto& assets  = scene_it->second.assets_by_key;
		auto asset_it = assets.find(asset_key);
		if (asset_it == assets.end()) {
			return;
		}

		if (asset_it->second.thumbnail != static_cast<ImTextureID>(0)) {
			// TODO: Fix.
			/*GLuint tex = static_cast<GLuint>(static_cast<uintptr_t>(asset_it->second.thumbnail));
			glDeleteTextures(1, &tex);
			asset_it->second.thumbnail = static_cast<ImTextureID>(0);*/
		}

		assets.erase(asset_it);
	}
};

static FakeAssetManager g_fake_asset_manager;

struct PendingImportedAssetPaths {
	std::mutex mutex;
	std::vector<std::string> paths;
};

static PendingImportedAssetPaths g_pending_imports;

static std::string GetCurrentSceneKey(EditorLayer& layer, const Application& app) {
	// TODO: Fix.
	// if (layer.current_scene_index_ >= 0 &&
	//	layer.current_scene_index_ < static_cast<int>(layer.scenes_.size())) {
	//	return layer.scenes_[layer.current_scene_index_].name;
	//}
	return "";
}

static ImTextureID LoadFakeThumbnailForPath(Application& app, const std::string& path) {
	(void)app;

	// TODO: Fix.
	return 0;

	/*
	SDL_Surface* loaded = IMG_Load(path.c_str());
	if (!loaded) {
		SDL_Log("IMG_Load failed for '%s': %s", path.c_str(), SDL_GetError());
		return static_cast<ImTextureID>(0);
	}

	// Convert to a format that maps cleanly to GL_RGBA / GL_UNSIGNED_BYTE.
	SDL_Surface* src = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_ABGR8888);
	SDL_DestroySurface(loaded);

	if (!src) {
		SDL_Log("SDL_ConvertSurface failed for '%s': %s", path.c_str(), SDL_GetError());
		return static_cast<ImTextureID>(0);
	}

	constexpr int kThumbSize = 128;

	SDL_Surface* thumb = SDL_CreateSurface(kThumbSize, kThumbSize, SDL_PIXELFORMAT_ABGR8888);
	if (!thumb) {
		SDL_Log("SDL_CreateSurface failed: %s", SDL_GetError());
		SDL_DestroySurface(src);
		return static_cast<ImTextureID>(0);
	}

	// Fill transparent first so aspect-fit images get transparent padding.
	if (!SDL_FillSurfaceRect(thumb, nullptr, SDL_MapSurfaceRGBA(thumb, 0, 0, 0, 0))) {
		SDL_Log("SDL_FillSurfaceRect failed: %s", SDL_GetError());
	}

	// Preserve aspect ratio inside a 128x128 thumbnail.
	const float src_w = static_cast<float>(src->w);
	const float src_h = static_cast<float>(src->h);
	const float scale = (src_w > 0.0f && src_h > 0.0f) ? SDL_min(
															 static_cast<float>(kThumbSize) / src_w,
															 static_cast<float>(kThumbSize) / src_h
														 )
													   : 1.0f;

	const int dst_w = SDL_max(1, static_cast<int>(src_w * scale));
	const int dst_h = SDL_max(1, static_cast<int>(src_h * scale));
	const int dst_x = (kThumbSize - dst_w) / 2;
	const int dst_y = (kThumbSize - dst_h) / 2;

	const SDL_Rect dst_rect = { dst_x, dst_y, dst_w, dst_h };

	if (!SDL_BlitSurfaceScaled(src, nullptr, thumb, &dst_rect, SDL_SCALEMODE_LINEAR)) {
		SDL_Log("SDL_BlitSurfaceScaled failed for '%s': %s", path.c_str(), SDL_GetError());
		SDL_DestroySurface(src);
		SDL_DestroySurface(thumb);
		return static_cast<ImTextureID>(0);
	}

	SDL_DestroySurface(src);

	GLuint tex = 0;
	glGenTextures(1, &tex);
	if (tex == 0) {
		SDL_Log("glGenTextures failed for '%s'", path.c_str());
		SDL_DestroySurface(thumb);
		return static_cast<ImTextureID>(0);
	}

	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glTexImage2D(
		GL_TEXTURE_2D, 0, GL_RGBA8, thumb->w, thumb->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, thumb->pixels
	);

	glBindTexture(GL_TEXTURE_2D, 0);

	SDL_DestroySurface(thumb);

	return static_cast<ImTextureID>(static_cast<intptr_t>(tex));
	*/
}

void DrawAssetsWindow(EditorLayer& layer, Application& app) {
	ImGui::Begin("Assets", nullptr, ImGuiWindowFlags_NoCollapse);

	const std::string current_scene_key = GetCurrentSceneKey(layer, app);
	if (current_scene_key.empty()) {
		ImGui::TextUnformatted("No scene selected.");
		ImGui::End();
		return;
	}

	// Consume any files selected from the SDL dialog.
	{
		std::vector<std::string> imported_paths;
		{
			std::lock_guard<std::mutex> lock(g_pending_imports.mutex);
			imported_paths.swap(g_pending_imports.paths);
		}

		for (const std::string& path : imported_paths) {
			ImTextureID thumb = static_cast<ImTextureID>(0);
			if (FakeAssetManager::IsImageFile(path)) {
				thumb = LoadFakeThumbnailForPath(app, path);
			}
			g_fake_asset_manager.AddAssetToScene(current_scene_key, path, thumb);
		}
	}

	FakeSceneAssetRegistry& registry =
		g_fake_asset_manager.GetOrCreateSceneRegistry(current_scene_key);

	ImGui::Text("Scene: %s", current_scene_key.c_str());
	ImGui::SameLine();
	ImGui::TextDisabled("| %d assets", static_cast<int>(registry.assets_by_key.size()));

	ImGui::Separator();

	static int items_per_row = 4;

	enum class AssetSortMode {
		Name,
		Type,
	};

	static AssetSortMode sort_mode = AssetSortMode::Name;
	static bool sort_ascending	   = true;

	if (ImGui::Button("Import...", ImVec2(120.0f, 0.0f))) {
		// TODO: Fix.
		/*const auto result = app.window_.file.OpenFiles({
			.filters = {
				{ "Images", "png,jpg,jpeg,bmp,tga,gif,webp" },
				{ "Audio", "wav,ogg,mp3" },
				{ "Scenes", "scene,json" },
				{ "All files", "*" },
			},
		});*/
	}

	ImGui::SameLine();
	ImGui::SetNextItemWidth(150.0f);
	ImGui::SliderInt("##ItemsPerRow", &items_per_row, 1, 8, "Items Per Row: %d");

	ImGui::SameLine();
	if (ImGui::Button("Sort: Name")) {
		if (sort_mode == AssetSortMode::Name) {
			sort_ascending = !sort_ascending; // toggle
		} else {
			sort_mode	   = AssetSortMode::Name;
			sort_ascending = true; // default direction
		}
	}

	ImGui::SameLine();
	if (ImGui::Button("Sort: Type")) {
		if (sort_mode == AssetSortMode::Type) {
			sort_ascending = !sort_ascending; // toggle
		} else {
			sort_mode	   = AssetSortMode::Type;
			sort_ascending = true;
		}
	}

	ImGui::Spacing();

	std::vector<FakeAssetInfo*> sorted_assets;
	sorted_assets.reserve(registry.assets_by_key.size());
	for (auto& [key, asset] : registry.assets_by_key) {
		sorted_assets.push_back(&asset);
	}

	std::stable_sort(
		sorted_assets.begin(), sorted_assets.end(),
		[&](const FakeAssetInfo* a, const FakeAssetInfo* b) {
			auto compare_name = [&](const FakeAssetInfo* x, const FakeAssetInfo* y) {
				return x->key < y->key;
			};

			auto compare_type = [&](const FakeAssetInfo* x, const FakeAssetInfo* y) {
				std::string x_ext = std::filesystem::path(x->path).extension().string();
				std::string y_ext = std::filesystem::path(y->path).extension().string();

				if (x_ext != y_ext) {
					return x_ext < y_ext;
				}
				return x->key < y->key;
			};

			bool result;
			if (sort_mode == AssetSortMode::Type) {
				result = compare_type(a, b);
			} else {
				result = compare_name(a, b);
			}

			return sort_ascending ? result : !result;
		}
	);

	ImGui::BeginChild(
		"##AssetsScrollRegion", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_AlwaysVerticalScrollbar
	);

	constexpr float kTilePadding		= 10.0f;
	constexpr float kKeyEditHeight		= 28.0f;
	constexpr float kFileTextHeight		= 20.0f;
	constexpr float kRemoveButtonHeight = 22.0f;

	const float avail_width = ImGui::GetContentRegionAvail().x;
	const int column_count	= (items_per_row < 1) ? 1 : items_per_row;

	const float total_spacing = (column_count - 1) * kTilePadding;
	const float tile_width	  = (avail_width - total_spacing) / static_cast<float>(column_count);

	const float preview_size = tile_width;
	const float tile_height =
		preview_size + kRemoveButtonHeight + kKeyEditHeight + kFileTextHeight + 12.0f;

	if (ImGui::BeginTable(
			"##AssetsGrid", column_count,
			ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoBordersInBody |
				ImGuiTableFlags_PadOuterX
		)) {
		std::string asset_to_remove;
		std::string asset_key_to_rename;
		std::string asset_new_key;

		for (FakeAssetInfo* asset : sorted_assets) {
			ImGui::TableNextColumn();
			ImGui::PushID(asset->key.c_str());

			ImGui::BeginGroup();

			if (asset->is_image && asset->thumbnail != static_cast<ImTextureID>(0)) {
				ImGui::Image(asset->thumbnail, ImVec2(preview_size, preview_size));
			} else {
				ImGui::Button("No Preview", ImVec2(preview_size, preview_size));
			}

			if (ImGui::Button("X", ImVec2(preview_size, 0.0f))) {
				asset_to_remove = asset->key;
			}

			{
				std::string key_buffer = asset->key;
				ImGui::SetNextItemWidth(preview_size);
				if (ImGui::InputText(
						"##AssetKey", key_buffer.data(), key_buffer.size(),
						ImGuiInputTextFlags_EnterReturnsTrue
					)) {
					std::string renamed_key = key_buffer;
					if (!renamed_key.empty() && renamed_key != asset->key) {
						asset_key_to_rename = asset->key;
						asset_new_key		= renamed_key;
					}
				}
			}

			{
				const std::string filename = std::filesystem::path(asset->path).filename().string();
				ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + preview_size);
				ImGui::TextDisabled("%s", filename.c_str());
				ImGui::PopTextWrapPos();
			}

			ImGui::EndGroup();

			const ImVec2 item_min	   = ImGui::GetItemRectMin();
			const ImVec2 item_max	   = ImGui::GetItemRectMax();
			const float current_height = item_max.y - item_min.y;
			if (current_height < tile_height) {
				ImGui::Dummy(ImVec2(0.0f, tile_height - current_height));
			}

			ImGui::PopID();
		}

		if (!asset_to_remove.empty()) {
			g_fake_asset_manager.RemoveAssetFromScene(current_scene_key, asset_to_remove);
		}

		if (!asset_key_to_rename.empty() && !asset_new_key.empty()) {
			auto it = registry.assets_by_key.find(asset_key_to_rename);
			if (it != registry.assets_by_key.end()) {
				FakeAssetInfo asset_info = it->second;
				registry.assets_by_key.erase(it);

				std::string unique_key = asset_new_key;
				int suffix			   = 1;
				while (registry.assets_by_key.find(unique_key) != registry.assets_by_key.end()) {
					unique_key = asset_new_key + "_" + std::to_string(suffix++);
				}

				asset_info.key					   = unique_key;
				registry.assets_by_key[unique_key] = asset_info;
			}
		}

		ImGui::EndTable();
	}

	ImGui::EndChild();
	ImGui::End();
}

} // namespace editor

} // namespace ptgn