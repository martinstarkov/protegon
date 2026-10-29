#pragma once

#include <string>

#include "commands/editor_command.h"
#include "commands/entity/entity_reference.h"

namespace ptgn::editor {

class Editor;

class RenameEntityCommand final : public EditorCommand {
public:
	RenameEntityCommand(
		Editor& editor,
		EntityReference entity,
		std::string before,
		std::string after
	);

	void Undo() override;
	void Redo() override;

	[[nodiscard]] std::string_view Label() const override;

private:
	void Apply(const std::string& name) const;

	Editor* editor_{ nullptr };
	EntityReference entity_;
	std::string before_;
	std::string after_;
};

} // namespace ptgn::editor
