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
	std::string scene_type;
	std::string display_name;
	std::string scene_tag{ "Main" };
	json params = json::object();
};

class SceneListPanel {
public:
	void OnRender(EditorContext& ctx);

	[[nodiscard]] Scene* GetSelectedScene() const;
	[[nodiscard]] const path& GetSelectedScenePath() const;

	void SetSelectedScene(EditorContext& ctx, Scene* scene, const path& scene_path = {});

	/// @brief Clears the current raw Scene pointer and selects the requested replacement once the
	/// deferred SceneManager command has been applied.
	void QueueSceneSelection(EditorContext& ctx, std::string scene_tag, bool runtime);

	bool ResolvePendingSceneSelection(EditorContext& ctx);
private:
	struct PendingSceneSelection {
		std::string tag;
		bool runtime{ false };
		path scene_path;
		int earliest_frame{ 0 };
	};

	void DrawSceneParamUI(EditorContext& ctx);

	Scene* selected_scene_{ nullptr };
	path selected_scene_path_;
	std::optional<SceneEditorState> state_;
	std::optional<PendingSceneSelection> pending_scene_selection_;
};

} // namespace editor

} // namespace ptgn
