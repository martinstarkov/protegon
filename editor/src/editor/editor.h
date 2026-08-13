#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>

#include "app/application_layer.h"
#include "app/application_state.h"
#include "app/project.h"
#include "commands/editor_commands.h"
#include "commands/undo_stack.h"
#include "editor/editor_context.h"
#include "editor/editor_settings.h"
#include "core/math/vector2.h"
#include "core/util/file.h"
#include "panels/content_browser.h"
#include "panels/inspector.h"
#include "panels/scene_hierarchy.h"
#include "panels/scene_list.h"
#include "panels/settings.h"
#include "panels/viewport.h"
#include "renderer/resources/id.h"
#include "runtime/scene/scene_file.h"

#if !defined(__EMSCRIPTEN__)
#include "editor/export_manager.h"
#endif

namespace ptgn {

class Application;
class Scene;
class Stats;
class Renderer;
class DebugSystem;
class AssetManager;
class Window;

namespace impl {

class SceneManager;

} // namespace impl

namespace editor {

class Editor : public ApplicationLayer {
public:
	explicit Editor(Application& app);

	void OnUpdate() override;
	void OnRender() override;

	const Window& GetWindow() const;
	Window& GetWindow();
	const AssetManager& GetAssetManager() const;
	AssetManager& GetAssetManager();
	const DebugSystem& GetDebugSystem() const;
	DebugSystem& GetDebugSystem();
	const ::ptgn::impl::SceneManager& GetSceneManager() const;
	::ptgn::impl::SceneManager& GetSceneManager();
	const Renderer& GetRenderer() const;
	Renderer& GetRenderer();
	std::size_t GetMaxTextureSlots() const;

	::ptgn::impl::TextureId GetPresentationTexture() const;
	V2_int GetPresentationTextureSize() const;

	void SetTimeScale(float time_scale);
	float GetTimeScale() const;
	void RequestStep();

	void SetApplicationState(ApplicationState state);
	ApplicationState GetApplicationState() const;

	void Play();
	void Stop();
	void TogglePause();
	void SaveProjectScene();

	SceneHierarchyPanel& GetSceneHierarchyPanel();
	SceneListPanel& GetSceneListPanel();

	void EnableRendering(bool enable = true);
	void OnSelectedSceneChanged(Scene* previous_scene, Scene* selected_scene);

	const EditorSettings& GetSettings() const;
	EditorSettings& GetSettings();
	void SetEditorSettings(EditorSettings settings);

	void SetEntityPickingMode(bool enabled);
	void SetGizmoUsesLocalOrientation(bool enabled);
	void SetRenderOnlySelectedScene(bool enabled);
	void SetSceneEntityPickingEnabled(Scene& scene, bool enabled);

	[[nodiscard]] Project* GetProject();
	[[nodiscard]] const Project* GetProject() const;

	void MarkProjectDirty();
	void RequestQuit();

	bool CreateProjectScene(std::string_view scene_type);
	bool DuplicateProjectScene(std::string_view scene_key);
	bool DeleteProjectScene(std::string_view scene_key);
	bool RenameProjectSceneKey(std::string_view current_key, std::string_view new_key);
	bool RenameProjectSceneDisplayName(std::string_view scene_key, std::string display_name);
	bool MoveProjectScene(std::size_t from_index, std::size_t to_index);
	bool SetStartupProjectScene(std::string_view scene_key);

	[[nodiscard]] bool IsStartupProjectScene(std::string_view scene_key) const;
	bool CanSaveProject() const;
	bool IsPlaying() const;
	bool IsPaused() const;
	bool CanPlay() const;
	bool CanStop() const;
	bool CanPause() const;
	bool IsDirectRuntime() const;
	std::optional<path> GetProjectRoot() const;

private:
	struct PlaySnapshot {
		std::string selected_scene_key;
		bool was_dirty{ false };
	};

#if !defined(__EMSCRIPTEN__)
	enum class PendingTaskConfirmation {
		None,
		Export,
		Clean,
		CloseExportWindowWhileRunning,
		CloseApplicationWhileRunning,
	};
#endif

	Application& app;

	void SavePendingBootstrapScenes();
	void SyncProjectSceneOrder();
	void UpdateProjectLocalState();
	void UpdateRuntimeViewportState();
	void SaveEditorLocalStateIfChanged();
	void OnProjectChanged();
	void RefreshProjectDirtyState();
	void UpdateWindowTitle();
	void DrawMainMenuBar();
	void DrawPanels();
	void BuildDefaultDockLayout(std::uint32_t dockspace_id);
	void UpdateDockLayout(std::uint32_t dockspace_id, float width);
	bool ShouldEnableEntityPicking() const;
	void ApplyEntityPickingSettings();
	void ApplySceneRenderSettings();
	void SyncSelectedSceneAssetDependencies();
	::ptgn::impl::FramebufferId GetSceneFramebuffer(Scene& scene) const;

#if !defined(__EMSCRIPTEN__)
	void OpenExportWindow();
	void DrawExportWindow();
	void DrawTaskConfirmationPopup();
	ExportRequest MakeExportRequest() const;
	bool ExportRequestNeedsConfirmation(const ExportRequest& request) const;
	void StartExport(ExportRequest request);
#endif

	std::unique_ptr<EditorContext> context_;
	UndoStack undo_stack_;
	EditorCommands commands_;
	ViewportPanel viewport_panel_;
	ContentBrowserPanel content_browser_panel_;
	SettingsWindow settings_window_;
	UndoHistoryWindow undo_history_window_;
	InspectorPanel inspector_panel_;
	SceneHierarchyPanel scene_hierarchy_panel_;
	SceneListPanel scene_list_panel_;
	std::optional<PlaySnapshot> play_snapshot_;
	std::optional<path> local_state_project_path_;
	std::optional<std::string> saved_editor_local_state_json_;
	std::uint32_t dock_left_column_id_{ 0 };
	std::uint32_t dock_right_column_id_{ 0 };
	std::uint8_t dock_resize_frames_remaining_{ 0 };
	bool dock_layout_update_requested_{ false };
	V2_float previous_dockspace_size_;
	bool dock_layout_built_{ false };
	bool scene_asset_dependencies_dirty_{ true };
	bool untracked_project_dirty_{ false };
	bool runtime_was_active_{ false };
	std::unordered_set<std::string> pending_scene_bootstrap_saves_;

#if !defined(__EMSCRIPTEN__)
	ExportManager export_manager_;
	bool export_window_open_{ false };
	bool export_window_recenter_requested_{ false };
	ExportTarget export_target_{ ExportTarget::Desktop };
	ExportConfiguration export_configuration_{ ExportConfiguration::Release };
	bool export_include_editor_{ false };
	std::string desktop_export_directory_;
	std::string web_export_directory_;
	PendingTaskConfirmation pending_task_confirmation_{ PendingTaskConfirmation::None };
	std::optional<ExportRequest> pending_export_request_;
	std::optional<ExportTarget> pending_clean_target_;
	std::optional<ExportConfiguration> pending_clean_configuration_;
	bool task_confirmation_popup_requested_{ false };
	bool allow_application_close_{ false };
#endif
};

} // namespace editor

} // namespace ptgn

#define PTGN_WITH_EDITOR(application, ...) \
	application.PushLayer<::ptgn::editor::Editor>(application).EnableRendering(__VA_ARGS__)
