#include "commands/entity/reparent_entity.h"

#include <utility>

#include "core/editor.h"
#include "runtime/ecs/entity_hierarchy.h"

namespace ptgn::editor {

ReparentEntityCommand::ReparentEntityCommand(
	Editor& editor,
	EntityReference child,
	std::optional<EntityReference> before_parent,
	std::optional<EntityReference> after_parent,
	std::optional<Transform> before_transform,
	std::optional<Transform> after_transform
) :
	editor_{ &editor },
	child_{ std::move(child) },
	before_parent_{ std::move(before_parent) },
	after_parent_{ std::move(after_parent) },
	before_transform_{ std::move(before_transform) },
	after_transform_{ std::move(after_transform) } {}

void ReparentEntityCommand::Undo() {
	Apply(before_parent_, before_transform_);
}

void ReparentEntityCommand::Redo() {
	Apply(after_parent_, after_transform_);
}

std::string_view ReparentEntityCommand::Label() const {
	return "Reparent Entity";
}

void ReparentEntityCommand::Apply(
	const std::optional<EntityReference>& parent,
	const std::optional<Transform>& transform
) const {
	Entity child{ child_.Resolve(*editor_) };
	if (!child) {
		return;
	}

	if (parent) {
		if (Entity resolved_parent{ parent->Resolve(*editor_) }) {
			SetParent(child, resolved_parent);
		}
	} else if (HasParent(child)) {
		RemoveParent(child);
	}

	if (transform && child.Has<Transform>()) {
		child.Get<Transform>() = *transform;
	}
}

} // namespace ptgn::editor
