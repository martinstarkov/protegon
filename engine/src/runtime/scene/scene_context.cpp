#include "runtime/scene/scene_context.h"

#include "app/application.h"
#include "core/event/dispatcher.h"
#include "runtime/scene/scene.h"

namespace ptgn {

SceneEventHandler::SceneEventHandler(Scene& scene) : scene_{ scene } {}

void SceneEventHandler::Emit(EventDispatcher d) {
	scene_.InternalEmit(d);
}

SceneContext::SceneContext(Application& app, Scene& scene) :
	global_event{ app.events_ },
	window{ app.window_ },
	asset{ app.assets_ },
	font{ app.font_ },
	audio{ app.audio_ },
	scene{ app.scenes_ },
	renderer{},
	debug{ renderer },
	event{ scene },
	input{ scene, app.input_ },
	physics{ scene },
	collision{},
	camera{} {
	renderer.Init(scene, app.renderer_);

	camera		 = CreateCamera(scene);
	fixed_camera = CreateCamera(scene);
	fixed_camera.SetMasks(kLayersNone, kLayersAll);

	SetUI(fixed_camera, true);
}

void SceneContext::Stop() {
	app_.running_ = false;
}

secondsf SceneContext::dt() const {
	return app_.dt_;
}

milliseconds SceneContext::TimeSinceStart() const {
	return app_.TimeSinceStart();
}

bool SceneContext::IsRunning() const {
	return app_.running_;
}

std::size_t SceneContext::GetFrameCount() const {
	return app_.frame_count_;
}

} // namespace ptgn