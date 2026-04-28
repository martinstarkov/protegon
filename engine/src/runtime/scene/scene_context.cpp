#include "runtime/scene/scene_context.h"

#include "app/application.h"
#include "core/util/time.h"
#include "runtime/scene/scene.h"

namespace ptgn {

SceneContext::SceneContext(Application& app, Scene& parent_scene) :
	window{ impl::ApplicationAccessor::ctx(app).window },
	asset{ impl::ApplicationAccessor::ctx(app).assets },
	font{ impl::ApplicationAccessor::ctx(app).font },
	audio{ impl::ApplicationAccessor::ctx(app).audio },
	scene{ impl::ApplicationAccessor::ctx(app).scene_manager, parent_scene },
	renderer{ parent_scene, impl::ApplicationAccessor::ctx(app).renderer },
	debug{ renderer },
	event{ impl::ApplicationAccessor::ctx(app).event_handler },
	input{ parent_scene, impl::ApplicationAccessor::ctx(app).window },
	physics{ parent_scene },
	global_renderer_{ impl::ApplicationAccessor::ctx(app).renderer },
	app_{ app } {}

SceneContext::~SceneContext() noexcept = default;

void SceneContext::Stop() {
	impl::ApplicationAccessor::ctx(app_).running = false;
}

secondsf SceneContext::dt() const {
	return impl::ApplicationAccessor::ctx(app_).dt;
}

milliseconds SceneContext::TimeSinceStart() const {
	return impl::ApplicationAccessor::ctx(app_).TimeSinceStart();
}

bool SceneContext::IsRunning() const {
	return impl::ApplicationAccessor::ctx(app_).running;
}

std::size_t SceneContext::GetFrameCount() const {
	return impl::ApplicationAccessor::ctx(app_).frame_count;
}

} // namespace ptgn