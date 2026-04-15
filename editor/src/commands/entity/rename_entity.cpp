#include "commands/entity/rename_entity.h"

#include <string>
#include <utility>

#include "runtime/ecs/entity.h"

namespace ptgn::editor {

RenameEntityCommand::RenameEntityCommand(Entity entity, std::string new_name) :
	entity_{ entity }, new_name_{ std::move(new_name) } {}

void RenameEntityCommand::Execute() {
	if (!entity_) {
		return;
	}

	// TODO: Use actual name component.
	old_name_ = entity_.Get<std::string>();
	entity_.Add<std::string>(new_name_);
}

void RenameEntityCommand::Undo() {
	if (!entity_) {
		return;
	}

	// TODO: Use actual name component.
	entity_.Add<std::string>(old_name_);
}

} // namespace ptgn::editor