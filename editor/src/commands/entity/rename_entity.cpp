#include "commands/entity/rename_entity.h"

#include <utility>

#include "editor/editor.h"
#include "runtime/ecs/tag.h"

namespace ptgn::editor {

RenameEntityCommand::RenameEntityCommand(
	Editor& editor,
	EntityReference entity,
	std::string before,
	std::string after
) :
	editor_{ &editor },
	entity_{ std::move(entity) },
	before_{ std::move(before) },
	after_{ std::move(after) } {}

void RenameEntityCommand::Undo() {
	Apply(before_);
}

void RenameEntityCommand::Redo() {
	Apply(after_);
}

std::string_view RenameEntityCommand::Label() const {
	return "Rename Entity";
}

void RenameEntityCommand::Apply(const std::string& name) const {
	if (Entity entity{ entity_.Resolve(*editor_) }) {
		entity.Get<Tag>().value = name;
	}
}

} // namespace ptgn::editor
