#pragma once

#include <string>

#include "commands/editor_command.h"
#include "commands/entity/entity_reference.h"
#include "commands/entity/entity_snapshot.h"
#include "editor/editor_selection.h"

namespace ptgn::editor {

class EditorContext;

class DeleteEntityCommand final : public EditorCommand {
public:
	DeleteEntityCommand(
		EditorContext& ctx,
		EntityReference entity,
		EntitySnapshot snapshot,
		EditorSelection before_selection,
		EditorSelection after_selection
	);

	void Undo() override;
	void Redo() override;

	[[nodiscard]] std::string_view Label() const override;

private:
	EditorContext* ctx_{ nullptr };
	EntityReference entity_;
	EntitySnapshot snapshot_;
	EditorSelection before_selection_;
	EditorSelection after_selection_;
};

} // namespace ptgn::editor
