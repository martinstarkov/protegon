#include "core/editor_context.h"

#include "app/project.h"
#include "serialization/json/json.h"
#include "serialization/json/json_file.h"

namespace ptgn::editor {

path GetEditorLocalStatePath(const Project& project) {
	auto path{ project.file_path };
	path.replace_extension(".ptgneditor");
	return path;
}

EditorLocalState LoadEditorLocalState(const Project& project) {
	const auto path{ GetEditorLocalStatePath(project) };

	if (!FileExists(path)) {
		return {};
	}

    json value = LoadJson(path);

    return value.get<EditorLocalState>();
}

void SaveEditorLocalState(
	const Project& project,
	const EditorLocalState& state
) {
	const auto path{ GetEditorLocalStatePath(project) };

    EnsureDirectory(path.parent_path());

    json value = state;

    SaveJson(value, path);
}

} // namespace ptgn::editor
