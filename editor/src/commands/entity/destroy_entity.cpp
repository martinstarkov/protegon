#include "commands/entity/destroy_entity.h"

#include <utility>

#include "core/editor_context.h"
#include "runtime/scene/scene.h"
#include "runtime/ui/button.h"
#include "runtime/ecs/entity_hierarchy.h"

namespace ptgn::editor {

DeleteEntityCommand::DeleteEntityCommand(
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

void DeleteEntityCommand::Undo() {
	if (auto* scene{ entity_.ResolveScene(ctx_->editor) }) {
		RestoreEntitySnapshot(*scene, snapshot_);
	}

	ApplyEditorSelection(*ctx_, before_selection_);
}

void DeleteEntityCommand::Redo() {
	if (auto* scene{ entity_.ResolveScene(ctx_->editor) }) {
		DestroyEntitySnapshot(*scene, snapshot_);
	}
	ApplyEditorSelection(*ctx_, after_selection_);
}

std::string_view DeleteEntityCommand::Label() const {
	return "Delete Entity";
}

} // namespace ptgn::editor
