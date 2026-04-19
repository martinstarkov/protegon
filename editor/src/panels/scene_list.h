#pragma once

#include "core/editor_context.h"
#include "core/util/file.h"

namespace ptgn {

class Scene;

namespace editor {

struct SceneEditorState {
	std::string scene_type_name;
	json params;
};

class SceneListPanel {
public:
	void OnRender(EditorContext& ctx);

	void SetSelectedScene(Scene* scene, const path& scene_path = {});

	Scene* GetSelectedScene() const;

private:
	void DrawSceneParamUI(EditorContext& ctx);

	path selected_scene_path_;
	Scene* selected_scene_{ nullptr };
	std::optional<SceneEditorState> state_;
};

} // namespace editor

} // namespace ptgn