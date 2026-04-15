#pragma once

#include "commands/editor_commands.h"
#include "commands/undo_stack.h"
#include "core/editor_selection.h"
#include "core/editor_state.h"

namespace ptgn::editor {

class Editor;

struct EditorContext {
	Editor& editor;
	EditorCommands& commands;
	UndoStack& undo;

	EditorSelection selection;
	EditorState state;
};

} // namespace ptgn::editor