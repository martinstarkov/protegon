#pragma once

#include <optional>
#include <string>

#include "core/util/file.h"
#include "serialization/json/json.h"

namespace ptgn {

class Scene;

namespace editor {

class Editor;
class EditorContext;

struct SceneEditorState {
	/// Stable key used by the scene registry and scene file.
	std::string scene_type;

	/// User-facing registered name.
	std::string display_name;

	/// Tag used when entering a new scene instance.
	std::string scene_tag{ "Main" };

	/// Reflected scene-member values.
	json params;
};

class SceneListPanel {
public:
	void OnRender(EditorContext& ctx);

	void SetSelectedScene(Editor& editor, Scene* scene, const path& scene_path = {});

	Scene* GetSelectedScene() const;

private:
	void DrawSceneParamUI(EditorContext& ctx);
	
	std::optional<SceneEditorState> state_;

	Scene* selected_scene_{ nullptr };
	path selected_scene_path_;

	std::optional<std::string> pending_selected_scene_tag_;
};

} // namespace editor

} // namespace ptgn