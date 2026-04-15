#pragma once

#include <memory>
#include <stack>

#include "commands/editor_command.h"

namespace ptgn::editor {

class Editor;

class UndoStack {
public:
	void Execute(std::unique_ptr<EditorCommand> cmd);

	void Undo();

	void Redo();

	[[nodiscard]] bool CanUndo() const;
	[[nodiscard]] bool CanRedo() const;

private:
	friend class Editor;

	void Clear();

	std::stack<std::unique_ptr<EditorCommand>> undo_stack_;
	std::stack<std::unique_ptr<EditorCommand>> redo_stack_;
};

} // namespace ptgn::editor