#pragma once

#include "core/util/file.h"
#include "platform/window_settings.h"
#include "renderer/renderer_settings.h"
#include "serialization/serialize.h"
#include "tools/debug/debug_settings.h"

namespace ptgn {

class Application;
struct Project;

/// @brief Settings tracked by the project manifest and shared by every user of the project.
struct ProjectSettings {
	WindowSettings window;
	RendererSettings renderer;
	DebugSettings debug;

	PTGN_REFLECT(
		ProjectSettings,
		window,
		renderer,
		debug
	)
};

/// @brief Machine/user specific engine state stored beside the project in .ptgnlocal.
struct ProjectLocalState {
	WindowLocalSettings window;

	PTGN_REFLECT(ProjectLocalState, window)
};

/// @return A snapshot of the current application settings that belong to the project.
ProjectSettings GetProjectSettings(Application& app);

/// @brief Applies project owned defaults to the active application systems.
void SetProjectSettings(
	Application& app,
	const ProjectSettings& settings
);

/// @return Path of the local engine state file stored beside the project manifest.
path GetProjectLocalStatePath(const Project& project);

/// @brief Loads machine/user specific engine state. Missing files and fields preserve defaults.
[[nodiscard]] ProjectLocalState LoadProjectLocalState(const Project& project);

/// @brief Writes machine/user specific engine state to the project .ptgnlocal file.
void SaveProjectLocalState(
	const Project& project,
	const ProjectLocalState& state
);

/// @return Current machine/user specific engine state.
ProjectLocalState GetProjectLocalState(Application& app);

/// @brief Applies machine/user specific overrides after project settings have been applied.
void SetProjectLocalState(
	Application& app,
	const ProjectLocalState& state
);

} // namespace ptgn
