#include "app/application_context.h"

#include <chrono>
#include <utility>
#include <variant>
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
#include "renderer/pipeline/scaling_mode.h"
#include "renderer/pipeline/viewport_event.h"
#include "serialization/json/json.h"

namespace ptgn {

namespace impl {

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
	event_handler{},
	window{ config.window,
			[this](impl::EventData&& event) {
				event_handler.global_event_queue_.emplace_back(std::move(event));
			} },
	renderer{ window,
			  [this](V2_int size, std::variant<ResizeType, impl::PresentationResizeType> type) {
				  if (std::holds_alternative<impl::PresentationResizeType>(type)) {
					  event_handler.Push<event::PresentationResized>(size);
					  return;
				  }
				  auto resize_type{ std::get<ResizeType>(type) };
				  switch (resize_type) {
					  case ResizeType::Display:
						  event_handler.Push<event::DisplayResized>(size);
						  break;
					  case ResizeType::Game: event_handler.Push<event::GameResized>(size); break;
					  default:				 PTGN_ERROR("Unknown ResizeType: ", std::to_underlying(resize_type));
				  }
			  } },
	assets{ renderer, audio, font },
	font{ assets },
	audio{ assets },
	debug{} {
	PTGN_INFO("Application Config: ", json(config));
}

ApplicationContext::~ApplicationContext() noexcept = default;

} // namespace impl

} // namespace ptgn