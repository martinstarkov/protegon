#include "app/application.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <chrono>
#include <format>
#include <string>

#include "core/assert.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "platform/glfw.h"
#include "platform/window.h"
#include "renderer/renderer.h"
#include "runtime/audio/audio_system.h"
#include "runtime/scene/scene_manager.h"
#include "tools/debug/debug_system.h"

#ifdef __EMSCRIPTEN__

#include <emscripten.h>
#include <emscripten/html5.h>

#include <memory>

EM_JS(int, get_screen_width, (), { return window.screen.width; });
EM_JS(int, get_screen_height, (), { return window.screen.height; });
EM_JS(double, get_device_pixel_ratio, (), { return window.devicePixelRatio || 1.0; });

#endif

namespace ptgn {

namespace impl {

#ifdef __EMSCRIPTEN__

static EM_BOOL EmscriptenResize(
	int event_type, const EmscriptenUiEvent* ui_event, void* window_ptr
) {
	if (!window_ptr) {
		return -1;
	}
	auto& window{ *static_cast<::ptgn::Window*>(window_ptr) };
	V2_int window_size{ ui_event->windowInnerWidth, ui_event->windowInnerHeight };
	// TODO: Figure out how to deal with itch.io fullscreen button not changing status to
	// fullscreen.
	V2_int screen_size{ get_screen_width(), get_screen_height() };
	if (window_size == screen_size) {
		auto device_pixel_ratio{ get_device_pixel_ratio() };
		window_size = window_size * device_pixel_ratio;
	}
	window.SetSize(window_size);
	return 0;
}

static EM_BOOL EmscriptenResizeMouseLeave(
	int event_type, const EmscriptenMouseEvent* mouse_event, void* window_ptr
) {
	if (!window_ptr) {
		return -1;
	}
	auto& window{ *static_cast<::ptgn::Window*>(window_ptr) };
	window.ClearInputState();
	return 0;
}

static void EmscriptenInit(Window& window) {
	emscripten_set_resize_callback(
		EMSCRIPTEN_EVENT_TARGET_WINDOW, static_cast<void*>(&window), 0, EmscriptenResize
	);
	emscripten_set_mouseleave_callback(
		EMSCRIPTEN_EVENT_TARGET_WINDOW, static_cast<void*>(&window), EM_TRUE,
		EmscriptenResizeMouseLeave
	);
}

void EmscriptenMainLoop(void* application) {
	auto& app{ *static_cast<Application*>(application) };

	app.Update();

	if (!app.running_) {
		emscripten_cancel_main_loop();
	}
}

#endif

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
	window_{ events_, renderer_, config.window },
	renderer_{ window_, events_ },
	scenes_{},
	events_{ scenes_ },
	assets_{ renderer_, audio_, font_ },
	font_{ assets_ },
	audio_{ assets_ },
	debug_{} {}

Application::Application(const std::string& title) :
	Application{ ApplicationConfig{ .window = { .title = title } } } {}

Application::Application(const std::string& title, V2_int window_size) :
	Application{ ApplicationConfig{ .window = { .title = title, .size = window_size } } } {}

Application::~Application() noexcept {
	// Requires access to destructors.
}

void Application::EnterMainLoop() {
	// Design decision: Latest possible point to show window is right before
	// loop starts. Comment this if you wish the window to appear hidden for an
	// indefinite period of time.
	window_.SetSetting(WindowSetting::Shown);
	running_ = true;

	renderer_.UpdateDisplayViewport(window_.GetSize(), true);

#ifdef __EMSCRIPTEN__
	impl::EmscriptenInit(window_);
	emscripten_set_main_loop_arg(
		impl::EmscriptenMainLoop, this, /*fps=*/0, /*simulateInfiniteLoop=*/true
	);
#else
	while (running_) {
		Update();
	}
#endif
}

void Application::Update() {
	debug_.PreUpdate();

	static auto start{ std::chrono::system_clock::now() };
	static auto end{ std::chrono::system_clock::now() };
	// Calculate time elapsed during previous frame.
	dt_ = end - start;

	// TODO: Consider fixed FPS vs dynamic: https://gafferongames.com/post/fix_your_timestep/.
	/*constexpr const float fps{ 60.0f };
	dt_ = 1.0f / fps;*/

	/*if (elapsed < dt_) {
		Delay(duration_cast<milliseconds>(dt_ - elapsed));
	}*/ // TODO: Add accumulator for when elapsed > dt (such as in Debug mode).

	start = end;

	running_ = window_.Update();

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	renderer_.BeginFrame();

	scenes_.Update(dt());

	audio_.Update();

	debug_.PostUpdate();

	for (const auto& layer : layers_) {
		layer->OnUpdate(*this);
	}

	renderer_.EndFrame();

	for (const auto& layer : layers_) {
		layer->OnRender(*this);
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