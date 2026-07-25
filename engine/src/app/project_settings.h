#pragma once

#include "renderer/renderer_settings.h"
#include "platform/window_settings.h"
#include "serialization/serialize.h"
#include "tools/debug/debug_settings.h"

namespace ptgn {

class Application;

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

/// @return A snapshot of the current application settings that belong to the project.
ProjectSettings GetProjectSettings(Application& app);

/// @brief Applies project owned settings to the active application systems.
void SetProjectSettings(
	Application& app,
	const ProjectSettings& settings
);

} // namespace ptgn