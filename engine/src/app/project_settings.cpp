#include "app/project_settings.h"

#include "app/application.h"
#include "app/application_context.h"
#include "app/project.h"
#include "core/util/file.h"
#include "platform/window.h"
#include "renderer/renderer.h"
#include "serialization/json/json.h"
#include "serialization/json/json_file.h"

namespace ptgn {

ProjectSettings GetProjectSettings(Application& app) {
	auto& context{ impl::ApplicationAccessor::ctx(app) };

	return ProjectSettings{
		.window = context.window.GetSettings(),
		.renderer = context.renderer.GetSettings(),
		.debug = context.debug.GetSettings(),
	};
}

void SetProjectSettings(
	Application& app,
	const ProjectSettings& settings
) {
	auto& context{ impl::ApplicationAccessor::ctx(app) };

	if (context.project.has_value()) {
		context.project->settings = settings;
	}

	// Apply the window first because renderer viewport sizing depends on it.
	context.window.SetSettings(settings.window);
	context.renderer.SetSettings(settings.renderer);
	context.debug.SetSettings(settings.debug);
}

path GetProjectLocalStatePath(const Project& project) {
	auto file_path{ project.file_path };
	file_path.replace_extension(".ptgnlocal");
	return file_path;
}

ProjectLocalState LoadProjectLocalState(const Project& project) {
	const auto file_path{ GetProjectLocalStatePath(project) };

	ProjectLocalState state;

	if (!FileExists(file_path)) {
		return state;
	}

	LoadJson(file_path).get_to(state);
	return state;
}

void SaveProjectLocalState(
	const Project& project,
	const ProjectLocalState& state
) {
	const auto file_path{ GetProjectLocalStatePath(project) };

	EnsureDirectory(file_path.parent_path());
    json value = state;
	SaveJson(value, file_path);
}

ProjectLocalState GetProjectLocalState(Application& app) {
	auto& context{ impl::ApplicationAccessor::ctx(app) };

	return ProjectLocalState{
		.window = context.window.GetLocalSettings(),
	};
}

void SetProjectLocalState(
	Application& app,
	const ProjectLocalState& state
) {
	auto& context{ impl::ApplicationAccessor::ctx(app) };
	context.window.SetLocalSettings(state.window);
}

} // namespace ptgn
