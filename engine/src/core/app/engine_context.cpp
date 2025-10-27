#include "core/app/engine_context.h"

#include "core/app/application.h"

namespace ptgn {

EngineContext EngineContext::Get(Application& app) {
	// TODO: Remove shader.
	return EngineContext{ &app.window_, &app.input_, &app.scene_,
						  &app.render_, &app.debug_, &app.shader };
}

} // namespace ptgn