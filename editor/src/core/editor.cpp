#include "core/editor.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

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
#include "core/math/vector2.h"
#include "core/util/hash.h"
#include "panels/content_browser.h"
#include "panels/inspector.h"
#include "panels/scene_hierarchy.h"
#include "panels/scene_list.h"
#include "panels/settings.h"
#include "panels/viewport.h"
#include "platform/window.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/renderer.h"
#include "renderer/resources/id.h"
#include "runtime/asset/asset_manager.h"
#include "app/project.h"
#include "runtime/scene/scene_file.h"
#include "runtime/scene/scene_manager.h"
#include "tools/debug/debug_system.h"

namespace ptgn::editor {

namespace {

constexpr float kLeftColumnRatio{ 0.25f };
constexpr float kRightColumnRatio{ 0.30f };

} // namespace

Editor::Editor(Application& app) : app{ app } {
	// Generic application startup preference. The engine does not know why it was changed.
	impl::ApplicationAccessor::ctx(app).start_project_runtime = false;

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

	DrawMainMenuBar();

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

void Editor::DrawMainMenuBar() {
	PTGN_ASSERT(context_);

	if (!ImGui::BeginMenuBar()) {
		return;
	}

	if (ImGui::BeginMenu("File")) {
		if (ImGui::MenuItem("Save", nullptr, false, !context_->state.is_playing)) {
			SaveProjectScene();
		}

		ImGui::EndMenu();
	}

	ImGui::EndMenuBar();
}

void Editor::DrawPanels() {
	if (scene_list_panel_.ResolvePendingSceneSelection(*this)) {
		auto* scene{ scene_list_panel_.GetSelectedScene() };

		viewport_panel_.SetUseEditorCamera(
			!scene || !scene->IsRuntime()
		);
	}

	// Needs to be rendered first so that the additional draw call can be displayed in the render
	// stats.
	viewport_panel_.OnRender(*context_);
	scene_hierarchy_panel_.OnRender(*context_);
	scene_list_panel_.OnRender(*context_);
	inspector_panel_.OnRender(*context_);
	engine_settings_panel_.OnRender(*context_);
	debug_settings_panel_.OnRender(*context_);
	editor_settings_panel_.OnRender(*context_);
	content_browser_panel_.OnRender(*context_);
}

const impl::SceneManager& Editor::GetSceneManager() const {
	return impl::ApplicationAccessor::ctx(app).scene_manager;
}

impl::SceneManager& Editor::GetSceneManager() {
	return impl::ApplicationAccessor::ctx(app).scene_manager;
}

SceneHierarchyPanel& Editor::GetSceneHierarchyPanel() {
	return scene_hierarchy_panel_;
}

SceneListPanel& Editor::GetSceneListPanel() {
	return scene_list_panel_;
}

bool Editor::ShouldEnableEntityPicking() const {
	PTGN_ASSERT(context_, "Editor context must be initialized");
	return context_->settings.entity_picking && render_enabled_;
}

impl::FramebufferId Editor::GetSceneFramebuffer(Scene& scene) const {
	auto render_target{ scene.GetRenderTarget() };

	return static_cast<impl::FramebufferId>(render_target.Get<impl::FramebufferObject>());
}

void Editor::OnSelectedSceneChanged(Scene* previous_scene, Scene* selected_scene) {
	if (previous_scene) {
		SetSceneEntityPickingEnabled(*previous_scene, false);
	}

	if (selected_scene) {
		SetSceneEntityPickingEnabled(*selected_scene, ShouldEnableEntityPicking());
	}
}

void Editor::EnableRendering(bool enable) {
	render_enabled_ = enable;

	auto& window{ GetWindow() };

	window.SetSetting(render_enabled_ ? WindowSetting::Maximized : WindowSetting::Restored);

	if (render_enabled_) {
		dock_layout_update_requested_ = true;

		// Maximizing may produce multiple window size updates.
		dock_resize_frames_remaining_ = 4;
	} else {
		auto& renderer{ GetRenderer() };

		renderer.SetPresentationViewport(std::nullopt);
		renderer.SetPrimaryWorldCamera(std::nullopt);
	}

	ApplyEntityPickingSettings();
}

void Editor::OnUpdate() {
	const auto& io{ ImGui::GetIO() };

	bool save_modifier{ io.KeyCtrl || io.KeySuper };

	if (save_modifier && ImGui::IsKeyPressed(ImGuiKey_S, false) &&
		!context_->state.is_playing) {
		SaveProjectScene();
	}

	if (ImGui::IsKeyPressed(ImGuiKey_F10)) {
		EnableRendering(!render_enabled_);
	}

	if (auto* scene{ scene_list_panel_.GetSelectedScene() }) {
		SetSceneEntityPickingEnabled(*scene, ShouldEnableEntityPicking());
	}
}

const EditorSettings& Editor::GetSettings() const {
	PTGN_ASSERT(context_, "Editor context must be initialized");
	return context_->settings;
}

void Editor::SetGizmoUsesLocalOrientation(bool enabled) {
	PTGN_ASSERT(context_, "Editor context must be initialized");

	context_->settings.gizmo_uses_local_orientation = enabled;
}

void Editor::SetEntityPickingMode(bool enabled) {
	PTGN_ASSERT(context_, "Editor context must be initialized");

	if (context_->settings.entity_picking == enabled) {
		return;
	}

	auto previously_enabled{ ShouldEnableEntityPicking() };

	context_->settings.entity_picking = enabled;

	auto currently_enabled{ ShouldEnableEntityPicking() };

	if (previously_enabled == currently_enabled) {
		return;
	}

	ApplyEntityPickingSettings();
}

void Editor::ApplyEntityPickingSettings() {
	auto* scene{ scene_list_panel_.GetSelectedScene() };

	if (!scene) {
		return;
	}

	SetSceneEntityPickingEnabled(*scene, ShouldEnableEntityPicking());
}

void Editor::Play() {
	PTGN_ASSERT(context_);

	if (context_->state.is_playing) {
		return;
	}

	auto* scene{ scene_list_panel_.GetSelectedScene() };
	if (!scene || scene->IsRuntime() || scene->GetRegisteredType().empty()) {
		return;
	}

	play_snapshot_ = PlaySnapshot{
		.scene_tag = scene->GetTag(),
		.scene = CaptureScene(*scene),
		.was_dirty = context_->state.is_dirty,
	};

	context_->selection.Clear();
	undo_stack_.Clear();

	SetApplicationState(ApplicationState::Running);

	if (!GetSceneManager().ReEnterFactory(
			play_snapshot_->scene_tag,
			impl::MakeSceneFactory(play_snapshot_->scene, true)
		)) {
		play_snapshot_.reset();
		return;
	}

	scene_list_panel_.QueueSceneSelection(
		*this,
		play_snapshot_->scene_tag,
		true
	);

	context_->state.is_playing = true;
	context_->state.is_paused = false;
}

void Editor::Stop() {
	PTGN_ASSERT(context_);

	if (!context_->state.is_playing || !play_snapshot_) {
		return;
	}

	context_->selection.Clear();
	SetApplicationState(ApplicationState::Running);

	std::string scene_tag{ play_snapshot_->scene_tag };
	auto factory{ impl::MakeSceneFactory(play_snapshot_->scene, false) };
	auto& manager{ GetSceneManager() };
	auto scene_hash{ Hash(scene_tag) };

	bool accepted{
		manager.HasScene(scene_hash)
			? manager.ReEnterFactory(scene_tag, std::move(factory))
			: manager.EnterFactory(scene_tag, std::move(factory))
	};

	if (!accepted) {
		return;
	}

	scene_list_panel_.QueueSceneSelection(
		*this,
		scene_tag,
		false
	);

	context_->state.is_playing = false;
	context_->state.is_paused = false;
	context_->state.is_dirty = play_snapshot_->was_dirty;
	play_snapshot_.reset();
}

void Editor::TogglePause() {
	PTGN_ASSERT(context_);

	if (!context_->state.is_playing) {
		return;
	}

	context_->state.is_paused = !context_->state.is_paused;
	SetApplicationState(
		context_->state.is_paused ? ApplicationState::Paused : ApplicationState::Running
	);
}

void Editor::SaveProjectScene() {
	PTGN_ASSERT(context_);

	if (context_->state.is_playing) {
		return;
	}

	auto& app_context{ impl::ApplicationAccessor::ctx(app) };
	auto* scene{ scene_list_panel_.GetSelectedScene() };
	if (!app_context.project || !scene || scene->IsRuntime() ||
		scene->GetRegisteredType().empty()) {
		return;
	}

	SaveSceneFile(GetStartupScenePath(app_context.project.value()), CaptureScene(*scene));
	context_->state.is_dirty = false;
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

Window& Editor::GetWindow() {
	return impl::ApplicationAccessor::ctx(app).window;
}

const Window& Editor::GetWindow() const {
	return impl::ApplicationAccessor::ctx(app).window;
}

const AssetManager& Editor::GetAssetManager() const {
	return impl::ApplicationAccessor::ctx(app).assets;
}

AssetManager& Editor::GetAssetManager() {
	return impl::ApplicationAccessor::ctx(app).assets;
}

const Renderer& Editor::GetRenderer() const {
	return impl::ApplicationAccessor::ctx(app).renderer;
}

Renderer& Editor::GetRenderer() {
	return impl::ApplicationAccessor::ctx(app).renderer;
}

DebugSystem& Editor::GetDebugSystem() {
	return impl::ApplicationAccessor::ctx(app).debug;
}

const DebugSystem& Editor::GetDebugSystem() const {
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
	return renderer.GetSize(texture).value();
}

void Editor::OnProjectChanged() {
	PTGN_ASSERT(context_, "Editor context must be initialized");
	context_->selection.Clear();

	undo_stack_.Clear();
	play_snapshot_.reset();

	context_->state.is_dirty = false;
	context_->state.is_playing = false;
	context_->state.is_paused = false;
}

void Editor::SetSceneEntityPickingEnabled(Scene& scene, bool enabled) {
	impl::RendererAccessor renderer{ GetRenderer() };

	for (auto [entity, framebuffer] : scene.EntitiesWith<impl::FramebufferObject>()) {
		renderer.SetEntityPickingEnabled(static_cast<impl::FramebufferId>(framebuffer), enabled);
	}
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
	ImGui::DockBuilderDockWindow("Editor Settings", dock_center_bottom);
	ImGui::DockBuilderDockWindow("Content Browser", dock_center_bottom);
	ImGui::DockBuilderDockWindow("Render Stats", dock_center_bottom);

	ImGui::DockBuilderFinish(dockspace_id);
}

} // namespace ptgn::editor