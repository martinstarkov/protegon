#pragma once

#include <imgui.h>

#include <memory>

#include "app/layer.h"
#include "commands/editor_commands.h"
#include "commands/undo_stack.h"
#include "core/editor_context.h"
#include "core/util/file.h"

namespace ptgn {

class Application;
class Scene;

namespace editor {

class Editor : public Layer {
public:
	explicit Editor(Application& app);

	void OnUpdate() override;
	void OnRender() override;

	void SetActiveScene(Scene* scene, path scene_path = {});

	Scene* GetActiveScene() const;

private:
	Application& app;

	void OnActiveSceneChanged();

	void DrawPanels();

	void BuildDefaultDockLayout(ImGuiID dockspace_id);

	std::unique_ptr<EditorContext> context_;
	UndoStack undo_stack_;
	EditorCommands commands_;

	bool dock_layout_built_{ false };
};

} // namespace editor

} // namespace ptgn

#define PTGN_WITH_EDITOR(application) application.PushLayer<editor::Editor>(application)