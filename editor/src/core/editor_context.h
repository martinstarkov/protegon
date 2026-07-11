#pragma once

#include "commands/editor_commands.h"
#include "commands/undo_stack.h"
#include "core/editor_selection.h"
#include "core/editor_settings.h"
#include "core/editor_state.h"

namespace ptgn::editor {

class Editor;

class EditorContext {
public:
	Editor& editor;
	EditorCommands& commands;
	UndoStack& undo;

	EditorSelection selection;
	EditorState state;
	EditorSettings settings;
};

} // namespace ptgn::editor