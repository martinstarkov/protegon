#include "app/project_settings.h"

#include "app/application.h"
#include "app/application_context.h"
#include "platform/window.h"
#include "renderer/renderer.h"

namespace ptgn {

ProjectSettings GetProjectSettings(Application& app) {
	auto& context{ impl::ApplicationAccessor::ctx(app) };
    
	ProjectSettings settings{
		.window = context.window.GetSettings(),
		.renderer = context.renderer.GetSettings(),
		.debug = context.debug.GetSettings(),
	};

	return settings;
}

void SetProjectSettings(
	Application& app,
	const ProjectSettings& settings
) {
	auto& context{ impl::ApplicationAccessor::ctx(app) };
    
	context.window.SetSettings(settings.window);
	context.renderer.SetSettings(settings.renderer);
    context.debug.SetSettings(settings.debug);
}

} // namespace ptgn