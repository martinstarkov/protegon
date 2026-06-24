#include "runtime/scene/scene_context.h"

#include <chrono>

#include "app/application.h"
#include "app/application_context.h"
#include "core/util/time.h"
#include "runtime/ecs/manager.h"
#include "runtime/graphics/render_queue.h"
#include "runtime/physics/physics.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_input.h"
#include "runtime/scene/scene_manager.h"

namespace ptgn {

namespace impl {

Application& SceneContextAccessor::app(SceneContext& ctx) {
	return ctx.app_;
}

const Application& SceneContextAccessor::app(const SceneContext& ctx) {
	return ctx.app_;
}

} // namespace impl

SceneContext::SceneContext(Application& app, Scene& parent_scene) :
	window{ impl::ApplicationAccessor::ctx(app).window },
	renderer{ impl::ApplicationAccessor::ctx(app).renderer },
	asset{ impl::ApplicationAccessor::ctx(app).assets },
	font{ impl::ApplicationAccessor::ctx(app).font },
	audio{ impl::ApplicationAccessor::ctx(app).audio },
	debug{ impl::ApplicationAccessor::ctx(app).debug },
	scene{ impl::ApplicationAccessor::ctx(app).scene_manager, parent_scene },
	render_queue{ impl::ApplicationAccessor::ctx(app).renderer, parent_scene },
	event{ impl::ApplicationAccessor::ctx(app).event_handler },
	input{ impl::ApplicationAccessor::ctx(app).window, parent_scene },
	physics{ parent_scene },
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

secondsf SceneContext::TimeSinceStartSeconds() const {
	return duration_cast<secondsf>(TimeSinceStart());
}

bool SceneContext::IsRunning() const {
	return impl::ApplicationAccessor::ctx(app_).running;
}

std::size_t SceneContext::GetFrameCount() const {
	return impl::ApplicationAccessor::ctx(app_).frame_count;
}

void SceneContext::Rebind(Scene& parent_scene) {
	scene.Rebind(parent_scene);
	render_queue.Rebind(parent_scene);
	input.Rebind(parent_scene);
	physics.Rebind(parent_scene);
}

} // namespace ptgn