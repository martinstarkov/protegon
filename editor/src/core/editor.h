#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>

#include "app/application_layer.h"
#include "app/application_state.h"
#include "commands/editor_commands.h"
#include "commands/undo_stack.h"
#include "core/editor_context.h"
#include "core/math/vector2.h"
#include "panels/content_browser.h"
#include "panels/inspector.h"
#include "panels/scene_hierarchy.h"
#include "panels/scene_list.h"
#include "panels/settings.h"
#include "panels/viewport.h"
#include "renderer/resources/id.h"
#include "runtime/scene/scene_file.h"

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

	::ptgn::impl::TextureId GetPresentationTexture() const;
	V2_int GetPresentationTextureSize() const;

	void SetTimeScale(float time_scale);
	float GetTimeScale() const;
	void RequestStep();

	void SetApplicationState(ApplicationState state);
	ApplicationState GetApplicationState() const;

	// Called by the viewport toolbar. These own the editor/runtime scene replacement lifecycle.
	void Play();
	void Stop();
	void TogglePause();
	void SaveProjectScene();

	SceneHierarchyPanel& GetSceneHierarchyPanel();
	SceneListPanel& GetSceneListPanel();

	void EnableRendering(bool enable = true);

	void OnSelectedSceneChanged(Scene* previous_scene, Scene* selected_scene);

	const EditorSettings& GetSettings() const;

	void SetEntityPickingMode(bool enabled);

	void SetGizmoUsesLocalOrientation(bool enabled);

	void SetSceneEntityPickingEnabled(Scene& scene, bool enabled);

	bool CreateProjectScene(
		std::string_view preferred_name,
		SerializedScene serialized_scene
	);

	bool DeleteProjectScene(
		std::string_view scene_tag
	);

	bool SetStartupProjectScene(
		std::string_view scene_tag
	);

	[[nodiscard]] bool IsStartupProjectScene(
		std::string_view scene_tag
	) const;

	bool CanSaveProject() const;

	bool IsPlaying() const;
private:
	struct PlaySnapshot {
		std::string scene_tag;
		SerializedScene scene;
		bool was_dirty{ false };
	};

	Application& app;

	void SavePendingBootstrapScenes();

	void OnProjectChanged();

	void DrawMainMenuBar();
	void DrawPanels();

	void BuildDefaultDockLayout(std::uint32_t dockspace_id);

	void UpdateDockLayout(std::uint32_t dockspace_id, float width);

	bool ShouldEnableEntityPicking() const;

	void ApplyEntityPickingSettings();

	::ptgn::impl::FramebufferId GetSceneFramebuffer(Scene& scene) const;

	std::unique_ptr<EditorContext> context_;
	UndoStack undo_stack_;
	EditorCommands commands_;

	ViewportPanel viewport_panel_;
	ContentBrowserPanel content_browser_panel_;
	EngineSettingsPanel engine_settings_panel_;
	DebugSettingsPanel debug_settings_panel_;
	EditorSettingsPanel editor_settings_panel_;
	InspectorPanel inspector_panel_;
	SceneHierarchyPanel scene_hierarchy_panel_;
	SceneListPanel scene_list_panel_;

	std::optional<PlaySnapshot> play_snapshot_;

	std::uint32_t dock_left_column_id_{ 0 };
	std::uint32_t dock_right_column_id_{ 0 };
	std::uint8_t dock_resize_frames_remaining_{ 0 };

	bool dock_layout_update_requested_{ false };
	V2_float previous_dockspace_size_;

	bool dock_layout_built_{ false };

	std::unordered_set<std::string> pending_scene_bootstrap_saves_;
};

} // namespace editor

} // namespace ptgn

/// @param Optional: bool argument to enable rendering on the editor layer. Defaults to true.
#define PTGN_WITH_EDITOR(application, ...) \
	application.PushLayer<::ptgn::editor::Editor>(application).EnableRendering(__VA_ARGS__)
