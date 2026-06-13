#include "app/application_context.h"

#include <chrono>
#include <utility>
#include <vector>

#include "app/application.h"
#include "application_config.h"
#include "core/assert.h"
#include "core/event/event.h"
#include "core/event/event_handler.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "core/util/time.h"
#include "platform/glfw.h"
#include "renderer/pipeline/viewport_event.h"
#include "renderer/renderer.h"
#include "serialization/json/json.h"
#include "tools/debug/debug_system.h"

namespace ptgn::impl {

ApplicationLibrary::ApplicationLibrary() {
	auto success{ glfwInit() };
	PTGN_ASSERT(success, "glfwInit failed");
	PTGN_INFO("Initialized GLFW");
}

ApplicationLibrary::~ApplicationLibrary() noexcept {
	glfwTerminate();
	PTGN_INFO("Deinitialized GLFW");
}

milliseconds ApplicationContext::TimeSinceStart() const {
	return duration_cast<milliseconds>(duration<double>{ glfwGetTime() });
}

ApplicationContext& ApplicationAccessor::ctx(Application& app) {
	return app.ctx_;
}

const ApplicationContext& ApplicationAccessor::ctx(const Application& app) {
	return app.ctx_;
}

ApplicationContext::ApplicationContext(const ApplicationConfig& config) :
	debug{},
	event_handler{},
	window{ config.window,
			[this](EventData&& event) {
				event_handler.global_event_queue_.emplace_back(std::move(event));
			} },
	renderer{ window, debug.stats,
			  [this](V2_int size, ResizeType type) {
				  switch (type) {
					  using enum ResizeType;
					  case Presentation:
						  event_handler.Push<event::PresentationResized>(size);
						  break;
					  case Display: event_handler.Push<event::DisplayResized>(size); break;
					  case Logical: event_handler.Push<event::LogicalResized>(size); break;
					  default:		PTGN_ERROR("Unknown ResizeType: ", std::to_underlying(type));
				  }
			  } },
	assets{ renderer, audio, font },
	font{ assets },
	audio{ assets } {
	PTGN_INFO("Application Config: ", json(config));
}

ApplicationContext::~ApplicationContext() noexcept = default;

} // namespace ptgn::impl