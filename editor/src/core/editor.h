#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "app/layer.h"
#include "commands/editor_commands.h"
#include "commands/undo_stack.h"
#include "core/editor_context.h"
#include "core/graphics/color.h"
#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "core/util/file.h"
#include "panels/content_browser.h"
#include "panels/engine_settings.h"
#include "panels/inspector.h"
#include "panels/scene_hierarchy.h"
#include "panels/scene_list.h"
#include "panels/viewport.h"
#include "renderer/pipeline/camera.h"
#include "renderer/pipeline/scaling_mode.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/resources/id.h"
#include "runtime/scene/scene_manager.h"

namespace ptgn {

class Application;
class Scene;

namespace editor {

class Editor : public Layer {
public:
	explicit Editor(Application& app);

	void OnUpdate() override;
	void OnRender() override;

	void SetScalingMode(ScalingMode scaling_mode);
	void SetGameSize(std::optional<V2_int> game_size);
	ScalingMode GetScalingMode() const;
	V2_int GetGameSize() const;
	bool HasGameSize() const;
	Viewport GetDisplayViewport() const;
	impl::TextureId GetScreenTargetTexture() const;
	void SetWindowBackgroundColor(Color color);
	Color GetWindowBackgroundColor() const;
	void SetRendererBackgroundColor(Color color);
	Color GetRendererBackgroundColor() const;

	const std::vector<std::unique_ptr<Scene>>& GetScenes() const;
	std::vector<std::unique_ptr<Scene>>& GetScenes();

	void SetPresentationViewport(Viewport viewport);

	SceneHierarchyPanel& GetSceneHierarchyPanel();
	SceneListPanel& GetSceneListPanel();

	impl::SceneManager& GetSceneManager();

	void SetPrimaryWorldCamera(const std::optional<Camera>& primary_world_camera);
	const std::optional<Camera>& GetPrimaryWorldCamera() const;

private:
	Application& app;

	void OnProjectChanged();

	void DrawPanels();

	void BuildDefaultDockLayout(std::uint32_t dockspace_id);

	std::unique_ptr<EditorContext> context_;
	UndoStack undo_stack_;
	EditorCommands commands_;

	ViewportPanel viewport_panel_;
	ContentBrowserPanel content_browser_panel_;
	EngineSettingsPanel engine_settings_panel_;
	InspectorPanel inspector_panel_;
	SceneHierarchyPanel scene_hierarchy_panel_;
	SceneListPanel scene_list_panel_;

	bool dock_layout_built_{ false };
};

} // namespace editor

} // namespace ptgn

#define PTGN_WITH_EDITOR(application) application.PushLayer<editor::Editor>(application)