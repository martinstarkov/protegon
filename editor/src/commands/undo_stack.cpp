#include "commands/undo_stack.h"

#include <memory>
#include <stack>
#include <utility>

#include "commands/editor_command.h"

namespace ptgn::editor {

void UndoStack::Execute(std::unique_ptr<EditorCommand> cmd) {
	cmd->Execute();

	undo_stack_.push(std::move(cmd));

	// Once you execute something new, redo history is invalid
	while (!redo_stack_.empty()) {
		redo_stack_.pop();
	}
}

void UndoStack::Undo() {
	if (undo_stack_.empty()) {
		return;
	}

	auto cmd{ std::move(undo_stack_.top()) };
	undo_stack_.pop();

	cmd->Undo();
	redo_stack_.push(std::move(cmd));
}

void UndoStack::Redo() {
	if (redo_stack_.empty()) {
		return;
	}

	auto cmd{ std::move(redo_stack_.top()) };
	redo_stack_.pop();

	cmd->Execute();
	undo_stack_.push(std::move(cmd));
}

bool UndoStack::CanUndo() const {
	return !undo_stack_.empty();
}

bool UndoStack::CanRedo() const {
	return !redo_stack_.empty();
}

void UndoStack::Clear() {
	redo_stack_ = {};
	undo_stack_ = {};
}

} // namespace ptgn::editor