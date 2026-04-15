#include "commands/entity/reparent_entity.h"

#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"

namespace ptgn::editor {

ReparentEntityCommand::ReparentEntityCommand(
	Entity child, Entity new_parent, bool ignore_parent_transform
) :
	child_{ child },
	new_parent_{ new_parent },
	ignore_parent_transform_{ ignore_parent_transform } {}

void ReparentEntityCommand::Execute() {
	if (!child_) {
		return;
	}

	old_had_parent_ = HasParent(child_);
	old_parent_		= GetParent(child_);

	if (new_parent_) {
		SetParent(child_, new_parent_, ignore_parent_transform_);
	} else {
		RemoveParent(child_);
	}
}

void ReparentEntityCommand::Undo() {
	if (!child_) {
		return;
	}

	if (old_had_parent_ && old_parent_) {
		SetParent(child_, old_parent_, ignore_parent_transform_);
	} else {
		RemoveParent(child_);
	}
}

} // namespace ptgn::editor