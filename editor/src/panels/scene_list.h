#pragma once

#include <optional>
#include <string>

#include "editor/renamable_item.h"
#include "runtime/ecs/uuid.h"
#include "serialization/json/json.h"

namespace ptgn {

class Scene;

namespace editor {

class EditorContext;

struct SceneEditorState {
	std::string scene_type{};
	json parameters = json::object();
};

class SceneListPanel {
public:
	void Bind(EditorContext& ctx);
	void OnRender(EditorContext& ctx);

	[[nodiscard]] Scene* GetSelectedScene() const;

	void SetSelectedScene(EditorContext& ctx, Scene* scene, bool undoable = true);

	/// Clears current selection and selects a deferred replacement scene later.
	void QueueSceneSelection(
		EditorContext& ctx,
		std::string scene_key,
		bool runtime,
		std::optional<UUID> selected_entity_uuid = std::nullopt
	);

	bool ResolvePendingSceneSelection(EditorContext& ctx);
	void ClearInvalidSceneSelection(EditorContext& ctx);
	void RefreshSelectedSceneState();

private:
	struct PendingSceneSelection {
		std::string key{};
		bool runtime{ false };
		std::optional<UUID> selected_entity_uuid{};
		int earliest_frame{ 0 };
	};

	void DrawSceneDetails(EditorContext& ctx);
	void DrawSceneKeyRenameModal(EditorContext& ctx);
	void RebuildSceneEditorState(Scene* scene);

	EditorContext* context_{ nullptr };
	std::optional<SceneEditorState> state_;
	std::optional<PendingSceneSelection> pending_scene_selection_;
	RenameModalState scene_key_rename_;
	std::optional<std::string> renaming_scene_key_;
};

} // namespace editor

} // namespace ptgn
