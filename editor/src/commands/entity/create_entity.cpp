#include "commands/entity/create_entity.h"

#include <utility>

#include "core/editor_context.h"
#include "runtime/scene/scene.h"

namespace ptgn::editor {

CreateEntityCommand::CreateEntityCommand(
	EditorContext& ctx,
	EntityReference entity,
	EntitySnapshot snapshot,
	EditorSelection before_selection,
	EditorSelection after_selection
) :
	ctx_{ &ctx },
	entity_{ std::move(entity) },
	snapshot_{ std::move(snapshot) },
	before_selection_{ std::move(before_selection) },
	after_selection_{ std::move(after_selection) } {}

void CreateEntityCommand::Undo() {
	if (auto* scene{ entity_.ResolveScene(ctx_->editor) }) {
		DestroyEntitySnapshot(*scene, snapshot_);
	}
	ApplyEditorSelection(*ctx_, before_selection_);
}

void CreateEntityCommand::Redo() {
	if (auto* scene{ entity_.ResolveScene(ctx_->editor) }) {
		(void)RestoreEntitySnapshot(*scene, snapshot_);
	}
	ApplyEditorSelection(*ctx_, after_selection_);
}

std::string_view CreateEntityCommand::Label() const {
	return "Create Entity";
}

} // namespace ptgn::editor
