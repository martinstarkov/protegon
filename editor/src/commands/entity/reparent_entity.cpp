#include "commands/entity/reparent_entity.h"

#include <utility>

#include "editor/editor.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/scene/scene_camera.h"

namespace ptgn::editor {

ReparentEntityCommand::ReparentEntityCommand(
	Editor& editor, EntityReference child, std::optional<EntityReference> before_parent,
	std::optional<EntityReference> after_parent, std::optional<Transform> before_transform,
	std::optional<Transform> after_transform, bool before_mask_override, bool after_mask_override
) :
	editor_{ &editor },
	child_{ std::move(child) },
	before_parent_{ std::move(before_parent) },
	after_parent_{ std::move(after_parent) },
	before_transform_{ std::move(before_transform) },
	after_transform_{ std::move(after_transform) },
	before_mask_override_{ before_mask_override },
	after_mask_override_{ after_mask_override } {}

void ReparentEntityCommand::Undo() {
	Apply(before_parent_, before_transform_, before_mask_override_);
}

void ReparentEntityCommand::Redo() {
	Apply(after_parent_, after_transform_, after_mask_override_);
}

std::string_view ReparentEntityCommand::Label() const {
	return "Reparent Entity";
}

void ReparentEntityCommand::Apply(
	const std::optional<EntityReference>& parent, const std::optional<Transform>& transform,
	bool mask_override
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

	if (mask_override) {
		child.TryAdd<::ptgn::impl::RenderMaskOverride>();
	} else {
		child.Remove<::ptgn::impl::RenderMaskOverride>();
	}

	if (transform && child.Has<Transform>()) {
		child.Get<Transform>() = *transform;
	}
}

} // namespace ptgn::editor
