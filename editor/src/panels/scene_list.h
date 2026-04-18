#pragma once

#include "core/editor_context.h"
#include "core/util/file.h"

namespace ptgn {

class Scene;

namespace editor {

class SceneListPanel {
public:
	void OnRender(EditorContext& ctx);

	void SetSelectedScene(Scene* scene, const path& scene_path = {});

	Scene* GetSelectedScene() const;

private:
	path selected_scene_path_;
	Scene* selected_scene_{ nullptr };
};

} // namespace editor

} // namespace ptgn