#include "core/editor.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cstdint>
#include <magic_enum/magic_enum.hpp>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "app/application.h"
#include "app/application_context.h"
#include "app/application_layer.h"
#include "app/application_state.h"
#include "commands/editor_commands.h"
#include "commands/undo_stack.h"
#include "core/assert.h"
#include "core/editor_context.h"
#include "core/editor_selection.h"
#include "core/editor_state.h"
#include "core/graphics/color.h"
#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "panels/content_browser.h"
#include "panels/engine_settings.h"
#include "panels/inspector.h"
#include "panels/scene_hierarchy.h"
#include "panels/scene_list.h"
#include "panels/viewport.h"
#include "platform/window.h"
#include "renderer/pipeline/camera.h"
#include "renderer/pipeline/scaling_mode.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/render_settings.h"
#include "renderer/renderer.h"
#include "renderer/resources/id.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_manager.h"
#include "tools/debug/debug_system.h"
#include "tools/debug/stats.h"

namespace ptgn::editor {

namespace {

constexpr float kLeftColumnRatio{ 0.25f };
constexpr float kRightColumnRatio{ 0.30f };

} // namespace

Editor::Editor(Application& app) : app{ app } {
	EditorSelection selection;

	EditorState state;

	state.is_dirty			= false;
	state.is_paused			= false;
	state.is_playing		= false;
	state.viewport.focused	= false;
	state.viewport.hovered	= false;
	state.viewport.viewport = {};

	context_ = std::make_unique<EditorContext>(
		*this, commands_, undo_stack_, std::move(selection), std::move(state)
	);

	commands_ = EditorCommands{ &undo_stack_, &scene_list_panel_ };
}

void Editor::OnUpdate() {
	if (ImGui::IsKeyPressed(ImGuiKey_F11)) {
		EnableRendering(!render_enabled_);

		auto& window{ impl::ApplicationAccessor::ctx(app).window };

		if (render_enabled_) {
			window.SetSetting(WindowSetting::Maximized);
		} else {
			window.SetSetting(WindowSetting::Restored);
		}

		dock_layout_update_requested_ = true;

		// Maximizing may produce multiple window size updates.
		dock_resize_frames_remaining_ = 4;
	}
}

void Editor::UpdateDockLayout(std::uint32_t dockspace_id, float width) {
	auto* dockspace{ ImGui::DockBuilderGetNode(dockspace_id) };

	if (!dockspace) {
		dock_layout_built_ = false;
		BuildDefaultDockLayout(dockspace_id);
		return;
	}

	ImGui::DockBuilderSetNodeSize(
		dockspace_id, ImVec2{
						  ImGui::GetMainViewport()->WorkSize.x,
						  ImGui::GetMainViewport()->WorkSize.y,
					  }
	);

	if (auto* left{ ImGui::DockBuilderGetNode(dock_left_column_id_) }) {
		left->SizeRef.x = width * kLeftColumnRatio;
	}

	if (auto* right{ ImGui::DockBuilderGetNode(dock_right_column_id_) }) {
		right->SizeRef.x = width * kRightColumnRatio;
	}
}

void Editor::OnRender() {
	auto* viewport{ ImGui::GetMainViewport() };

	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);

	auto window_flags{ ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
					   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
					   ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
					   ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_MenuBar };

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f });

	ImGui::Begin("EditorRootDockspace", nullptr, window_flags);

	ImGui::PopStyleVar(3);

	auto dockspace_id{ ImGui::GetID("EditorDockspace") };
	auto dockspace_size{ ImGui::GetContentRegionAvail() };

	BuildDefaultDockLayout(dockspace_id);

	if (dock_resize_frames_remaining_ > 0) {
		UpdateDockLayout(dockspace_id, dockspace_size.x);
		--dock_resize_frames_remaining_;
	}

	ImGui::DockSpace(dockspace_id, dockspace_size, ImGuiDockNodeFlags_None);

	DrawPanels();

	ImGui::End();
}

void Editor::DrawPanels() {
	// Needs to be rendered first so that the additional draw call can be displayed in the render
	// stats.
	viewport_panel_.OnRender(*context_);
	scene_hierarchy_panel_.OnRender(*context_);
	scene_list_panel_.OnRender(*context_);
	inspector_panel_.OnRender(*context_);
	engine_settings_panel_.OnRender(*context_);
	debug_settings_panel_.OnRender(*context_);
	content_browser_panel_.OnRender(*context_);
}

const std::vector<std::unique_ptr<Scene>>& Editor::GetScenes() const {
	return impl::ApplicationAccessor::ctx(app).scene_manager.GetScenes();
}

std::vector<std::unique_ptr<Scene>>& Editor::GetScenes() {
	return impl::ApplicationAccessor::ctx(app).scene_manager.GetScenes();
}

void Editor::SetPresentationViewport(std::optional<Viewport> presentation_viewport) {
	impl::ApplicationAccessor::ctx(app).renderer.SetPresentationViewport(presentation_viewport);
}

RenderSettings Editor::GetRenderSettings() const {
	return impl::ApplicationAccessor::ctx(app).renderer.GetSettings();
}

void Editor::SetRenderSettings(const RenderSettings& settings) {
	impl::ApplicationAccessor::ctx(app).renderer.SetSettings(settings);
}

SceneHierarchyPanel& Editor::GetSceneHierarchyPanel() {
	return scene_hierarchy_panel_;
}

SceneListPanel& Editor::GetSceneListPanel() {
	return scene_list_panel_;
}

impl::SceneManager& Editor::GetSceneManager() {
	return impl::ApplicationAccessor::ctx(app).scene_manager;
}

void Editor::SetPrimaryWorldCamera(const std::optional<Camera>& primary_world_camera) {
	impl::ApplicationAccessor::ctx(app).renderer.SetPrimaryWorldCamera(primary_world_camera);
}

const std::optional<Camera>& Editor::GetPrimaryWorldCamera() const {
	return impl::ApplicationAccessor::ctx(app).renderer.GetPrimaryWorldCamera();
}

const Stats& Editor::GetStats() const {
	return impl::ApplicationAccessor::ctx(app).debug.stats;
}

Stats& Editor::GetStats() {
	return impl::ApplicationAccessor::ctx(app).debug.stats;
}

Renderer& Editor::GetRenderer() {
	return impl::ApplicationAccessor::ctx(app).renderer;
}

void Editor::EnableRendering(bool enable) {
	render_enabled_ = enable;
	if (!render_enabled_) {
		SetPresentationViewport(std::nullopt);
		SetPrimaryWorldCamera(std::nullopt);
	}
}

void Editor::SetScalingMode(ScalingMode scaling_mode) {
	impl::ApplicationAccessor::ctx(app).renderer.SetScalingMode(scaling_mode);
}

void Editor::SetLogicalSize(std::optional<V2_int> logical_size) {
	impl::ApplicationAccessor::ctx(app).renderer.SetLogicalSize(logical_size, std::nullopt);
}

ScalingMode Editor::GetScalingMode() const {
	return impl::ApplicationAccessor::ctx(app).renderer.GetScalingMode();
}

bool Editor::HasLogicalSize() const {
	return impl::ApplicationAccessor::ctx(app).renderer.HasLogicalSize();
}

V2_int Editor::GetLogicalSize() const {
	return impl::ApplicationAccessor::ctx(app).renderer.GetLogicalSize();
}

Viewport Editor::GetDisplayViewport() const {
	return impl::ApplicationAccessor::ctx(app).renderer.GetDisplayViewport();
}

void Editor::SetWindowBackgroundColor(Color color) {
	impl::ApplicationAccessor::ctx(app).window.SetBackgroundColor(color);
}

Color Editor::GetWindowBackgroundColor() const {
	return impl::ApplicationAccessor::ctx(app).window.GetBackgroundColor();
}

void Editor::SetRendererBackgroundColor(Color color) {
	impl::ApplicationAccessor::ctx(app).renderer.SetBackgroundColor(color);
}

Color Editor::GetRendererBackgroundColor() const {
	return impl::ApplicationAccessor::ctx(app).renderer.GetBackgroundColor();
}

void Editor::SetTimeScale(float time_scale) {
	impl::ApplicationAccessor::ctx(app).time_scale = std::max(0.0f, time_scale);
}

float Editor::GetTimeScale() const {
	return impl::ApplicationAccessor::ctx(app).time_scale;
}

void Editor::RequestStep() {
	impl::ApplicationAccessor::ctx(app).step_requested = true;
}

DebugSystem& Editor::GetDebug() {
	return impl::ApplicationAccessor::ctx(app).debug;
}

const DebugSystem& Editor::GetDebug() const {
	return impl::ApplicationAccessor::ctx(app).debug;
}

void Editor::SetApplicationState(ApplicationState state) {
	impl::ApplicationAccessor::ctx(app).state = state;
}

ApplicationState Editor::GetApplicationState() const {
	return impl::ApplicationAccessor::ctx(app).state;
}

impl::TextureId Editor::GetPresentationTexture() const {
	impl::RendererAccessor renderer{ impl::ApplicationAccessor::ctx(app).renderer };
	auto texture{ renderer.GetPresentationTexture() };
	return texture;
}

V2_int Editor::GetPresentationTextureSize() const {
	impl::RendererAccessor renderer{ impl::ApplicationAccessor::ctx(app).renderer };
	auto texture{ renderer.GetPresentationTexture() };
	return renderer.GetSize(texture);
}

void Editor::OnProjectChanged() {
	PTGN_ASSERT(context_, "Editor context must be initialized");
	context_->selection.Clear();

	undo_stack_.Clear();

	context_->state.is_dirty = false;
}

void Editor::BuildDefaultDockLayout(std::uint32_t dockspace_id) {
	if (dock_layout_built_) {
		return;
	}

	dock_layout_built_ = true;

	auto* viewport{ ImGui::GetMainViewport() };
	auto work_size{ viewport->WorkSize };

	ImGui::DockBuilderRemoveNode(dockspace_id);
	ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
	ImGui::DockBuilderSetNodeSize(dockspace_id, work_size);

	ImGuiID dock_main{ dockspace_id };

	dock_left_column_id_ = ImGui::DockBuilderSplitNode(
		dock_main, ImGuiDir_Left, kLeftColumnRatio, nullptr, &dock_main
	);

	// The right split ratio is relative to the space remaining after
	// removing the left column.
	float right_split_ratio{ kRightColumnRatio / (1.0f - kLeftColumnRatio) };

	dock_right_column_id_ = ImGui::DockBuilderSplitNode(
		dock_main, ImGuiDir_Right, right_split_ratio, nullptr, &dock_main
	);

	ImGuiID dock_left{};
	ImGuiID dock_left_bottom{};

	ImGui::DockBuilderSplitNode(
		dock_left_column_id_, ImGuiDir_Down, 0.35f, &dock_left_bottom, &dock_left
	);

	ImGuiID dock_right{};

	// ImGuiID dock_right_bottom{};
	ImGui::DockBuilderSplitNode(dock_right_column_id_, ImGuiDir_Down, 0.35f, nullptr, &dock_right);

	ImGuiID dock_center_bottom{};

	ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Down, 0.25f, &dock_center_bottom, &dock_main);

	ImGui::DockBuilderDockWindow("Scene Hierarchy###SceneHierarchyWindow", dock_left);
	ImGui::DockBuilderDockWindow("Scenes", dock_left_bottom);

	ImGui::DockBuilderDockWindow("Inspector", dock_right);

	ImGui::DockBuilderDockWindow("Viewport", dock_main);

	ImGui::DockBuilderDockWindow("Engine Settings", dock_center_bottom);
	ImGui::DockBuilderDockWindow("Debug Settings", dock_center_bottom);
	ImGui::DockBuilderDockWindow("Render Stats", dock_center_bottom);
	ImGui::DockBuilderDockWindow("Render Graph", dock_center_bottom);
	ImGui::DockBuilderDockWindow("Content Browser", dock_center_bottom);

	ImGui::DockBuilderFinish(dockspace_id);
}

} // namespace ptgn::editor