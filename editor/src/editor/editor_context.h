#pragma once

#include "commands/editor_commands.h"
#include "commands/undo_stack.h"
#include "editor/editor_position_picker.h"
#include "editor/editor_selection.h"
#include "editor/editor_settings.h"
#include "editor/editor_state.h"
#include "core/util/file.h"
#include "serialization/serialize.h"

namespace ptgn {

struct Project;

namespace editor {

class Editor;

struct EditorLocalState {
	EditorSettings settings;
	EditorState state;
	EditorSelection selection;

	/// Runtime editor state. This is intentionally not serialized.
	PositionPicker position_picker;

	PTGN_REFLECT(EditorLocalState, settings, state, selection)
};

class EditorContext {
public:
	Editor& editor;
	EditorCommands& commands;
	UndoStack& undo;

	EditorLocalState local;
};

/// @return Path of the local editor state file stored beside the project manifest.
path GetEditorLocalStatePath(const Project& project);

/// @brief Loads local editor state. Missing files and fields preserve defaults.
[[nodiscard]] EditorLocalState LoadEditorLocalState(const Project& project);

/// @brief Writes local editor state to the project specific local file.
void SaveEditorLocalState(const Project& project, const EditorLocalState& state);

} // namespace editor

} // namespace ptgn
