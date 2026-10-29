#pragma once

#include <optional>

#include "commands/editor_command.h"
#include "commands/entity/entity_reference.h"
#include "core/math/transform.h"

namespace ptgn::editor {

class Editor;

class ReparentEntityCommand final : public EditorCommand {
public:
	ReparentEntityCommand(
		Editor& editor, EntityReference child, std::optional<EntityReference> before_parent,
		std::optional<EntityReference> after_parent, std::optional<Transform> before_transform,
		std::optional<Transform> after_transform, bool before_mask_override,
		bool after_mask_override
	);

	void Undo() override;
	void Redo() override;

	[[nodiscard]] std::string_view Label() const override;

private:
	void Apply(
		const std::optional<EntityReference>& parent, const std::optional<Transform>& transform,
		bool mask_override
	) const;

	Editor* editor_{ nullptr };
	EntityReference child_;
	std::optional<EntityReference> before_parent_;
	std::optional<EntityReference> after_parent_;
	std::optional<Transform> before_transform_;
	std::optional<Transform> after_transform_;
	bool before_mask_override_{ false };
	bool after_mask_override_{ false };
};

} // namespace ptgn::editor
