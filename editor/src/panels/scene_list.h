#pragma once

#include <optional>
#include <string>

#include "serialization/json/json.h"

namespace ptgn {

class Scene;

namespace editor {

class Editor;
class EditorContext;

struct SceneEditorState {
	std::string scene_type;
	std::string display_name;
	std::string scene_tag{ "Main" };
	json params = json::object();
};

class SceneListPanel {
public:
	void OnRender(EditorContext& ctx);

	[[nodiscard]] Scene* GetSelectedScene() const;

	void SetSelectedScene(
		Editor& editor,
		Scene* scene
	);

	void QueueSceneSelection(
		Editor& editor,
		std::string scene_tag,
		bool runtime
	);

	bool ResolvePendingSceneSelection(Editor& editor);
private:
	struct PendingSceneSelection {
		std::string tag;
		bool runtime{ false };
		int earliest_frame{ 0 };
	};

	void DrawSceneParamUI(EditorContext& ctx);

	Scene* selected_scene_{ nullptr };
	std::optional<SceneEditorState> state_;
	std::optional<PendingSceneSelection> pending_scene_selection_;
};

} // namespace editor

} // namespace ptgn