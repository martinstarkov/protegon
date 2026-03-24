#include "runtime/scene/scene_context.h"

#include "app/application.h"
#include "core/event/dispatcher.h"
#include "core/time/time.h"
#include "runtime/graphics/camera.h"
#include "runtime/scene/scene.h"

namespace ptgn {

SceneEventHandler::SceneEventHandler(Scene& scene) : scene_{ scene } {}

void SceneEventHandler::Emit(EventDispatcher d) {
	scene_.InternalEmit(d);
}

SceneContext::SceneContext(Application& app, Scene& parent_scene) :
	global_event{ app.events_ },
	window{ app.window_ },
	asset{ app.assets_ },
	font{ app.font_ },
	audio{ app.audio_ },
	scene{ app.scenes_ },
	renderer{ parent_scene, app.renderer_ },
	debug{ renderer },
	event{ parent_scene },
	input{ parent_scene, app.input_ },
	physics{ parent_scene },
	collision{},
	camera{ CreateCamera(parent_scene) },
	fixed_camera_{ CreateCamera(parent_scene) },
	global_renderer_{ app.renderer_ },
	app_{ app } {
	fixed_camera_.SetMasks(kLayersNone, kLayersAll);
	SetUI(fixed_camera_, true);
}

SceneContext::~SceneContext() noexcept {
	// Needs access to destructors.
}

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