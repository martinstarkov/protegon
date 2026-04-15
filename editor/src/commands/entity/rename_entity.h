#pragma once

#include <string>

#include "commands/editor_command.h"
#include "runtime/ecs/entity.h"

namespace ptgn::editor {

class RenameEntityCommand : public EditorCommand {
public:
	RenameEntityCommand(Entity entity, std::string new_name);

	void Execute() override;
	void Undo() override;

private:
	Entity entity_;
	std::string new_name_;
	std::string old_name_;
};

} // namespace ptgn::editor