#pragma once

#include <string>
#include <vector>

#include "commands/editor_commands.h"
#include "commands/undo_stack.h"
#include "editor/editor_position_picker.h"
#include "editor/editor_selection.h"
#include "editor/editor_settings.h"
#include "editor/editor_state.h"
#include "core/graphics/color.h"
#include "core/util/file.h"
#include "serialization/json/fwd.h"
#include "serialization/serialize.h"

namespace ptgn {

struct Project;

namespace editor {

class Editor;

struct EditorColorPalette {
	std::string name{ "Palette" };
	std::vector<Color> colors{};

	bool operator==(const EditorColorPalette&) const = default;
};

/// @brief Explicit JSON serialization keeps palette files tolerant of empty palettes and malformed
/// individual color entries instead of routing Color's array representation through reflected
/// object deserialization.
void to_json(json& value, const EditorColorPalette& palette);
void from_json(const json& value, EditorColorPalette& palette);

/// @brief Editor-only project data shared by every user of the project.
/// Stored beside the project manifest in the tracked .ptgneditor file.
struct EditorProjectState {
	std::vector<EditorColorPalette> color_palettes{};

	bool operator==(const EditorProjectState&) const = default;
};

void to_json(json& value, const EditorProjectState& state);
void from_json(const json& value, EditorProjectState& state);

/// @brief User/machine-specific editor state stored inside the editor object of .ptgnlocal.
struct EditorLocalState {
	EditorSettings settings{};
	EditorState state{};
	EditorSelection selection{};

	/// @brief Runtime editor state. This is intentionally not serialized.
	PositionPicker position_picker{};

	PTGN_REFLECT(EditorLocalState, settings, state, selection)
};

class EditorContext {
public:
	Editor& editor;
	EditorCommands& commands;
	UndoStack& undo;

	EditorLocalState local{};
	EditorProjectState project_state{};
};

/// @return Path of the shared editor project-state file stored beside the project manifest.
path GetEditorProjectStatePath(const Project& project);

/// @brief Loads editor-only project data shared by all users. Missing/invalid fields preserve
/// defaults. Invalid individual palette colors are ignored instead of aborting project startup.
[[nodiscard]] EditorProjectState LoadEditorProjectState(const Project& project);

/// @brief Writes shared editor-only project data to the project .ptgneditor file.
void SaveEditorProjectState(const Project& project, const EditorProjectState& state);

/// @brief Loads this user's editor state from the editor object in .ptgnlocal.
/// Legacy .ptgneditor local-state files are read as a migration fallback.
[[nodiscard]] EditorLocalState LoadEditorLocalState(const Project& project);

/// @brief Writes this user's editor state into the editor object in .ptgnlocal while preserving
/// other local state such as window geometry.
void SaveEditorLocalState(const Project& project, const EditorLocalState& state);

} // namespace editor

} // namespace ptgn
