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
		Editor& editor,
		EntityReference child,
		std::optional<EntityReference> before_parent,
		std::optional<EntityReference> after_parent,
		std::optional<Transform> before_transform,
		std::optional<Transform> after_transform
	);

	void Undo() override;
	void Redo() override;

	[[nodiscard]] std::string_view Label() const override;

private:
	void Apply(
		const std::optional<EntityReference>& parent,
		const std::optional<Transform>& transform
	) const;

	Editor* editor_{ nullptr };
	EntityReference child_;
	std::optional<EntityReference> before_parent_;
	std::optional<EntityReference> after_parent_;
	std::optional<Transform> before_transform_;
	std::optional<Transform> after_transform_;
};

} // namespace ptgn::editor
