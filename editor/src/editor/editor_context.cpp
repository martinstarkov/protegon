#include "editor/editor_context.h"

#include "app/project.h"
#include "serialization/json/json.h"
#include "serialization/json/json_file.h"

namespace ptgn::editor {

path GetEditorLocalStatePath(const Project& project) {
	auto file_path{ project.file_path };
	file_path.replace_extension(".ptgneditor");
	return file_path;
}

EditorLocalState LoadEditorLocalState(const Project& project) {
	const auto file_path{ GetEditorLocalStatePath(project) };

	EditorLocalState state;

	if (!FileExists(file_path)) {
		return state;
	}

	LoadJson(file_path).get_to(state);
	return state;
}

void SaveEditorLocalState(
	const Project& project,
	const EditorLocalState& state
) {
	const auto file_path{ GetEditorLocalStatePath(project) };

	EnsureDirectory(file_path.parent_path());
    json value = state;
	SaveJson(value, file_path);
}

} // namespace ptgn::editor
