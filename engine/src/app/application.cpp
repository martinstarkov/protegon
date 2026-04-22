#include "app/application.h"

#ifdef __EMSCRIPTEN__

#include <emscripten.h>

#endif

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <chrono>
#include <functional>
#include <memory>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "app/layer.h"
#include "core/assert.h"
#include "core/event/event.h"
#include "core/event/event_handler.h"
#include "core/event/window_event.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "core/util/time.h"
#include "platform/glfw.h"
#include "platform/window.h"
#include "renderer/pipeline/scaling_mode.h"
#include "renderer/pipeline/viewport_event.h"
#include "renderer/renderer.h"
#include "runtime/audio/audio_system.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_manager.h"
#include "serialization/json/fwd.h"
#include "tools/debug/debug_system.h"

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

} // namespace impl

Application::Application(const ApplicationConfig& config) :
	event_handler_{},
	window_{ config.window,
			 [this](impl::EventData&& event) {
				 event_handler_.global_event_queue_.emplace_back(std::move(event));
			 } },
	renderer_{ window_,
			   [this](V2_int size, std::variant<ResizeType, impl::PresentationResizeType> type) {
				   if (std::holds_alternative<impl::PresentationResizeType>(type)) {
					   event_handler_.Push<event::PresentationResized>(size);
					   return;
				   }
				   auto resize_type{ std::get<ResizeType>(type) };
				   switch (resize_type) {
					   case ResizeType::Display:
						   event_handler_.Push<event::DisplayResized>(size);
						   break;
					   case ResizeType::Game: event_handler_.Push<event::GameResized>(size); break;
					   default:				  PTGN_ERROR("Unknown ResizeType: ", std::to_underlying(resize_type));
				   }
			   } },
	assets_{ renderer_, audio_, font_ },
	font_{ assets_ },
	audio_{ assets_ },
	debug_{} {
	PTGN_INFO("Application Config: ", json(config));
}

Application::Application(std::string_view title) :
	Application{ ApplicationConfig{ .window{ .title{ title } } } } {}

Application::Application(std::string_view title, V2_int window_size) :
	Application{ ApplicationConfig{ .window{ .title{ title }, .size{ window_size } } } } {}

Application::~Application() noexcept = default;

void Application::EnterMainLoop() {
	// Design decision: Latest possible point to show window is right before
	// loop starts. Comment this if you wish the window to appear hidden for an
	// indefinite period of time.
	window_.SetSetting(WindowSetting::Shown);
	running_ = true;

	renderer_.UpdateDisplayViewport(true);

#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop_arg(
		[](void* application) {
			auto& app{ *static_cast<Application*>(application) };

			app.Update();

			if (!app.running_) {
				emscripten_cancel_main_loop();
			}
		},
		this, /*fps=*/0, /*simulateInfiniteLoop=*/true
	);
#else
	while (running_) {
		Update();
	}
#endif
}

void Application::HandleGlobalEvents() {
	auto global_events{ std::exchange(event_handler_.global_event_queue_, {}) };

	for (auto& global_event : global_events) {
		Event event{ global_event };
		event.Dispatch<event::WindowResized>([this](const auto& size) {
			renderer_.OnWindowResize(size);
		});
		for (const auto& scene : scene_manager_.GetScenes()) {
			if (scene->IsAwaitingTransitionDelay()) {
				continue;
			}
			scene->InternalOnEvent(event);
		}
	}
}

void Application::Update() {
	debug_.PreUpdate();

	static auto start{ std::chrono::system_clock::now() };
	static auto end{ std::chrono::system_clock::now() };
	// Calculate time elapsed during previous frame.
	dt_ = end - start;

	// TODO: Consider fixed FPS vs dynamic: https://gafferongames.com/post/fix_your_timestep/.
	constexpr float kFps{ 60.0f };

	if (dt_ > secondsf{ 1.0f / kFps }) {
		// TODO: Instead of clamping, consider using an accumulator to update multiple times if dt
		// is large (such as in Debug mode).
		dt_ = secondsf{ 1.0f / kFps };
	}

	start = end;

	running_ = window_.PollEvents();

	if (window_.GetSetting(WindowSetting::Minimized)) {
		audio_.Update();
		debug_.PostUpdate();

		end = std::chrono::system_clock::now();
		frame_count_++;
		return;
	}

	renderer_.UpdateDisplayViewport();

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	scene_manager_.PreUpdate();

	HandleGlobalEvents();

	scene_manager_.OnEvent();

	scene_manager_.Update(*this, dt());

	for (const auto& layer : layers_) {
		layer->OnUpdate();
	}

	audio_.Update();

	debug_.PostUpdate();

	renderer_.BeginFrame();
	scene_manager_.Draw();
	renderer_.EndFrame();

	for (const auto& layer : layers_) {
		layer->OnRender();
	}

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	window_.SwapBuffers();

	end = std::chrono::system_clock::now();
	frame_count_++;
}

milliseconds Application::TimeSinceStart() const {
	return duration_cast<milliseconds>(duration<double>{ glfwGetTime() });
}

void Application::Stop() {
	running_ = false;
}

secondsf Application::dt() const {
	return dt_;
}

bool Application::IsRunning() const {
	return running_;
}

std::size_t Application::GetFrameCount() const {
	return frame_count_;
}

} // namespace ptgn