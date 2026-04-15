#include "core/editor.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <filesystem>
#include <memory>
#include <utility>
#include <vector>

#include "app/application.h"
#include "commands/editor_commands.h"
#include "commands/undo_stack.h"
#include "core/assert.h"
#include "core/editor_context.h"
#include "core/editor_selection.h"
#include "core/editor_state.h"
#include "core/math/vector2.h"
#include "core/util/file.h"
#include "panels/content_browser.h"
#include "panels/engine_settings.h"
#include "panels/inspector.h"
#include "panels/scene_hierarchy.h"
#include "panels/scene_list.h"
#include "panels/viewport.h"
#include "renderer/renderer.h"
#include "renderer/resources/id.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_manager.h"

namespace ptgn::editor {

Editor::Editor(Application& app) : app{ app } {
	auto active_scene{ app.scene_manager_.GetScenes().empty()
						   ? nullptr
						   : app.scene_manager_.GetScenes().front().get() };

	EditorSelection selection;

	EditorState state;

	state.active_scene		= active_scene;
	state.active_scene_path = path{};
	state.is_dirty			= false;
	state.is_paused			= false;
	state.is_playing		= false;
	state.viewport.focused	= false;
	state.viewport.hovered	= false;
	state.viewport.viewport = {};

	context_ = std::make_unique<EditorContext>(
		*this, commands_, undo_stack_, std::move(selection), std::move(state)
	);

	commands_ = EditorCommands{ &undo_stack_, &context_->state };
}

void Editor::OnUpdate() {}

void Editor::OnRender() {
	ImGuiViewport* viewport = ImGui::GetMainViewport();

	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);

	ImGuiWindowFlags window_flags =
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_MenuBar;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f });

	ImGui::Begin("EditorRootDockspace", nullptr, window_flags);

	ImGui::PopStyleVar(3);

	ImGuiID dockspace_id = ImGui::GetID("EditorDockspace");

	ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
	ImGui::DockSpace(dockspace_id, ImVec2{ 0.0f, 0.0f }, dockspace_flags);

	BuildDefaultDockLayout(dockspace_id);

	DrawPanels();

	ImGui::End();
}

void Editor::DrawPanels() {
	viewport_panel_.OnRender(*context_);
	scene_hierarchy_panel_.OnRender(*context_);
	scene_list_panel_.OnRender(*context_);
	inspector_panel_.OnRender(*context_);
	engine_settings_panel_.OnRender(*context_);
	content_browser_panel_.OnRender(*context_);
}

void Editor::SetActiveScene(Scene* scene, std::filesystem::path scene_path) {
	PTGN_ASSERT(context_, "Editor context must be initialized");

	if (context_->state.active_scene == scene) {
		return;
	}

	context_->state.active_scene	  = scene;
	context_->state.active_scene_path = std::move(scene_path);

	OnActiveSceneChanged();
}

Scene* Editor::GetActiveScene() const {
	PTGN_ASSERT(context_, "Editor context must be initialized");
	return context_->state.active_scene;
}

V2_int Editor::GetDisplaySize() const {
	return app.renderer_.GetDisplaySize();
}

ImTextureID Editor::GetScreenTargetTexture() const {
	auto texture{ app.renderer_.GetRenderTargetTexture(app.renderer_.GetScreenTarget()) };
	return static_cast<ImTextureID>(texture.value);
}

void Editor::OnActiveSceneChanged() {
	PTGN_ASSERT(context_, "Editor context must be initialized");
	context_->selection.Clear();

	undo_stack_.Clear();

	context_->state.is_dirty = false;
}

void Editor::BuildDefaultDockLayout(ImGuiID dockspace_id) {
	if (dock_layout_built_) {
		return;
	}

	dock_layout_built_ = true;

	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImVec2 work_size		= viewport->WorkSize;

	ImGui::DockBuilderRemoveNode(dockspace_id);
	ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
	ImGui::DockBuilderSetNodeSize(dockspace_id, work_size);

	ImGuiID dock_main		   = dockspace_id;
	ImGuiID dock_left		   = 0;
	ImGuiID dock_right		   = 0;
	ImGuiID dock_left_bottom   = 0;
	ImGuiID dock_right_bottom  = 0;
	ImGuiID dock_center_bottom = 0;

	const float left_ratio			= 0.20f;
	const float right_ratio			= 0.22f;
	const float left_bottom_ratio	= 0.35f;
	const float right_bottom_ratio	= 0.35f;
	const float center_bottom_ratio = 0.25f;

	dock_left =
		ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left, left_ratio, nullptr, &dock_main);

	dock_right =
		ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Right, right_ratio, nullptr, &dock_main);

	dock_left_bottom = ImGui::DockBuilderSplitNode(
		dock_left, ImGuiDir_Down, left_bottom_ratio, nullptr, &dock_left
	);

	dock_right_bottom = ImGui::DockBuilderSplitNode(
		dock_right, ImGuiDir_Down, right_bottom_ratio, nullptr, &dock_right
	);

	dock_center_bottom = ImGui::DockBuilderSplitNode(
		dock_main, ImGuiDir_Down, center_bottom_ratio, nullptr, &dock_main
	);

	ImGui::DockBuilderDockWindow("Scene Hierarchy###SceneHierarchyWindow", dock_left);
	ImGui::DockBuilderDockWindow("Scenes", dock_left_bottom);

	ImGui::DockBuilderDockWindow("Inspector", dock_right);
	ImGui::DockBuilderDockWindow("Engine Settings", dock_right_bottom);

	ImGui::DockBuilderDockWindow("Game", dock_main);
	ImGui::DockBuilderDockWindow("Assets", dock_center_bottom);

	ImGui::DockBuilderFinish(dockspace_id);
}

} // namespace ptgn::editor