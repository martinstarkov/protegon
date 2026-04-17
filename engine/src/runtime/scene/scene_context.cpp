#include "runtime/scene/scene_context.h"

#include "app/application.h"
#include "core/util/time.h"
#include "runtime/scene/scene.h"

namespace ptgn {

SceneContext::SceneContext(Application& app, Scene& parent_scene) :
	window{ app.window_ },
	asset{ app.assets_ },
	font{ app.font_ },
	audio{ app.audio_ },
	scene{ app.scene_manager_, parent_scene },
	renderer{ parent_scene, app.renderer_ },
	debug{ renderer },
	event{ app.event_handler_ },
	input{ parent_scene, app.window_ },
	physics{ parent_scene },
	global_renderer_{ app.renderer_ },
	app_{ app } {}

SceneContext::~SceneContext() noexcept = default;

void SceneContext::Stop() {
	app_.Stop();
}

secondsf SceneContext::dt() const {
	return app_.dt();
}

milliseconds SceneContext::TimeSinceStart() const {
	return app_.TimeSinceStart();
}

bool SceneContext::IsRunning() const {
	return app_.IsRunning();
}

std::size_t SceneContext::GetFrameCount() const {
	return app_.GetFrameCount();
}

} // namespace ptgn