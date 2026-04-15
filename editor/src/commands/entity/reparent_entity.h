#pragma once

#include "commands/editor_command.h"
#include "runtime/ecs/entity.h"

namespace ptgn::editor {

class ReparentEntityCommand : public EditorCommand {
public:
	ReparentEntityCommand(Entity child, Entity new_parent, bool ignore_parent_transform = false);

	void Execute() override;
	void Undo() override;

private:
	Entity child_;
	Entity new_parent_;
	Entity old_parent_;

	bool ignore_parent_transform_{ false };
	bool old_had_parent_{ false };
};

} // namespace ptgn::editor