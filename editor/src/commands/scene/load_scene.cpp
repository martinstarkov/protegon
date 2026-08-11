#include "commands/scene/load_scene.h"

#include <utility>

#include "editor/editor.h"
#include "editor/editor_context.h"
#include "panels/scene_list.h"
#include "runtime/scene/scene_manager.h"

namespace ptgn::editor {

LoadSceneCommand::LoadSceneCommand(
	EditorContext& ctx,
	std::string scene_key,
	bool runtime,
	SerializedScene before,
	SerializedScene after
) :
	ctx_{ &ctx },
	scene_key_{ std::move(scene_key) },
	runtime_{ runtime },
	before_{ std::move(before) },
	after_{ std::move(after) } {}

void LoadSceneCommand::Undo() {
	Apply(before_);
}

void LoadSceneCommand::Redo() {
	Apply(after_);
}

std::string_view LoadSceneCommand::Label() const {
	return "Load Scene";
}

void LoadSceneCommand::Apply(const SerializedScene& scene) const {
	auto& manager{ ctx_->editor.GetSceneManager() };
	if (!manager.ReEnterFactory(
			scene_key_,
			impl::MakeSceneFactory(scene, runtime_)
		)) {
		return;
	}

	ctx_->editor.GetSceneListPanel().QueueSceneSelection(
		*ctx_,
		scene_key_,
		runtime_,
		ctx_->local.selection.GetEntityUUID(scene_key_, runtime_)
	);
}

} // namespace ptgn::editor
