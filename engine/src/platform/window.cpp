#include "platform/window.h"

#ifdef __EMSCRIPTEN__

#include <emscripten.h>
#include <emscripten/html5.h>

EM_JS(int, get_canvas_width, (), { return Module.canvas.width; });
EM_JS(int, get_canvas_height, (), { return Module.canvas.height; });

#endif

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <expected>
#include <ios>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/event/event.h"
#include "core/event/key_event.h"
#include "core/event/mouse_event.h"
#include "core/event/window_event.h"
#include "core/graphics/color.h"
#include "core/graphics/surface.h"
#include "core/input/key.h"
#include "core/input/mouse.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "core/util/file.h"
#include "platform/glfw.h"
#include "renderer/backend/gl/gl.h"
#include "renderer/renderer.h"

namespace ptgn {

#ifdef __EMSCRIPTEN__

void Window::SetCanvasSize(V2_int new_size) {
	emscripten_set_element_css_size("#canvas", new_size.x, new_size.y);
}

V2_int Window::GetCanvasSize() const {
	return { get_canvas_width(), get_canvas_height() };
}

#endif

namespace impl {

void WindowDeleter::operator()(GLFWwindow* window) const {
	if (window) {
		glfwDestroyWindow(window);
		PTGN_INFO("Destroyed window");
	}
}

void CursorDeleter::operator()(GLFWcursor* cursor) const {
	if (cursor) {
		glfwDestroyCursor(cursor);
	}
}

} // namespace impl

void Window::SetCallbacks() {
	auto win{ instance_.get() };
	PTGN_ASSERT(win != nullptr);

	glfwSetWindowPosCallback(win, [](GLFWwindow* window, int x, int y) {
		auto self{ static_cast<Window*>(glfwGetWindowUserPointer(window)) };
		if (!self) {
			return;
		}

		V2_int pos{ x, y };
		if (glfwGetWindowMonitor(window) == nullptr) {
			self->windowed_pos_ = pos;
		}
		self->PushEvent<event::WindowMoved>(pos);
	});

	glfwSetWindowMaximizeCallback(win, [](GLFWwindow* window, int maximized) {
		auto self{ static_cast<Window*>(glfwGetWindowUserPointer(window)) };
		if (!self) {
			return;
		}
		if (maximized) {
			self->PushEvent<event::WindowMaximized>(self->windowed_size_);
		}
	});

	glfwSetWindowIconifyCallback(win, [](GLFWwindow* window, int iconified) {
		auto self{ static_cast<Window*>(glfwGetWindowUserPointer(window)) };
		if (!self) {
			return;
		}
		if (iconified) {
			self->PushEvent<event::WindowMinimized>(self->windowed_size_);
		}
	});

	glfwSetWindowSizeCallback(win, [](GLFWwindow* window, int width, int height) {
		auto self{ static_cast<Window*>(glfwGetWindowUserPointer(window)) };
		if (!self) {
			return;
		}

		if (glfwGetWindowMonitor(window) == nullptr) {
			self->windowed_size_ = { width, height };
			self->windowed_was_maximized_ =
				glfwGetWindowAttrib(window, GLFW_MAXIMIZED) == GLFW_TRUE;
		}

		V2_int size{ width, height };

		self->PushEvent<event::WindowResized>(size);
	});

	glfwSetWindowFocusCallback(win, [](GLFWwindow* window, int focused) {
		auto self{ static_cast<Window*>(glfwGetWindowUserPointer(window)) };
		if (!self) {
			return;
		}
		if (focused) {
			self->focused_ = true;
			self->PushEvent<event::WindowFocusGained>();
		} else {
			self->ClearInputState();
			self->focused_ = false;
			self->PushEvent<event::WindowFocusLost>();
		}
	});

	glfwSetWindowCloseCallback(win, [](GLFWwindow* window) {
		auto self{ static_cast<Window*>(glfwGetWindowUserPointer(window)) };
		if (!self) {
			return;
		}

		self->quit_ = true;
		self->PushEvent<event::WindowQuit>();
	});

	glfwSetKeyCallback(
		win,
		[](GLFWwindow* window, int key, [[maybe_unused]] int scancode, int action,
		   [[maybe_unused]] int mods) {
			auto self{ static_cast<Window*>(glfwGetWindowUserPointer(window)) };
			if (!self || key < 0 || static_cast<std::size_t>(key) >= self->key_states_.size()) {
				return;
			}

			switch (action) {
				case GLFW_PRESS: {
					self->key_down_[static_cast<std::size_t>(key)] = true;
					break;
				}
				case GLFW_RELEASE: {
					self->key_down_[static_cast<std::size_t>(key)] = false;
					break;
				}
				case GLFW_REPEAT:
					// Ignore for physical state.
					// The key is already down.
					break;
				default: PTGN_ERROR("Unknown key action: ", action);
			}
		}
	);

	// TODO: In the future add unicode callback.
	// glfwSetCharCallback(win, [](GLFWwindow* window, unsigned int keycode) {
	//	auto self{ static_cast<Window*>(glfwGetWindowUserPointer(window)) };
	//	if (!self) {
	//		return;
	//	}
	//});

	glfwSetMouseButtonCallback(
		win,
		[](GLFWwindow* window, int button, int action, [[maybe_unused]] int mods) {
			auto self{ static_cast<Window*>(glfwGetWindowUserPointer(window)) };
			if (!self || button < 0 ||
				static_cast<std::size_t>(button) >= self->mouse_states_.size()) {
				return;
			}

			switch (action) {
				case GLFW_PRESS: {
					self->mouse_down_[static_cast<std::size_t>(button)] = true;
					break;
				}
				case GLFW_RELEASE: {
					self->mouse_down_[static_cast<std::size_t>(button)] = false;
					break;
				}
				default: PTGN_ERROR("Unknown mouse action: ", action);
			}
		}
	);

	glfwSetScrollCallback(win, [](GLFWwindow* window, double x, double y) {
		auto self{ static_cast<Window*>(glfwGetWindowUserPointer(window)) };
		if (!self) {
			return;
		}

		self->raw_scroll_accum_ += V2_float{ x, y };
	});

	glfwSetCursorPosCallback(win, [](GLFWwindow* window, double x, double y) {
		auto self{ static_cast<Window*>(glfwGetWindowUserPointer(window)) };
		if (!self) {
			return;
		}

		self->raw_mouse_position_ = { x, y };
	});
}

Window::Window(const WindowConfig& config) : file{ *this }, title_{ config.title } {
	int exclusive_states = static_cast<int>(config.minimized) + static_cast<int>(config.maximized) +
						   static_cast<int>(config.fullscreen);

	PTGN_ASSERT(
		exclusive_states <= 1,
		"Window config can only be one of fullscreen, minimized or maximized at once"
	);

	glfwDefaultWindowHints();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);
	glfwWindowHint(GLFW_DECORATED, config.borderless ? GLFW_FALSE : GLFW_TRUE);
	glfwWindowHint(GLFW_FLOATING, config.always_on_top ? GLFW_TRUE : GLFW_FALSE);
	glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, config.transparent ? GLFW_TRUE : GLFW_FALSE);
	glfwWindowHint(GLFW_MAXIMIZED, config.maximized ? GLFW_TRUE : GLFW_FALSE);

	GLFWmonitor* monitor{ nullptr };
	if (config.fullscreen) {
		monitor = glfwGetPrimaryMonitor();
		PTGN_ASSERT(monitor != nullptr, "glfwGetPrimaryMonitor failed");
	}

#ifdef __EMSCRIPTEN__
	const char* glsl_version = "#version 300 es";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#else
	const char* glsl_version = "#version 330 core";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
#endif

	float main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
	instance_		 = std::unique_ptr<GLFWwindow, impl::WindowDeleter>{
		   glfwCreateWindow(config.size.x, config.size.y, title_.c_str(), monitor, nullptr),
		   impl::WindowDeleter{}
	};

	PTGN_ASSERT(instance_ != nullptr, "glfwCreateWindow failed");

	glfwMakeContextCurrent(instance_.get());
	glfwSwapInterval(1); // Enable vsync

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	(void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	ImGui::StyleColorsDark();

	// Setup scaling
	ImGuiStyle& style = ImGui::GetStyle();
	style.ScaleAllSizes(main_scale);
	style.FontScaleDpi = main_scale;

#ifndef __EMSCRIPTEN__
	int status{ gladLoadGL(glfwGetProcAddress) };
	PTGN_ASSERT(status, "Failed to load OpenGL functions");
#endif

	PTGN_INFO("Graphics Card: ", glGetString(GL_RENDERER));
	PTGN_INFO("OpenGL Version: ", glGetString(GL_VERSION));

	glfwSetWindowUserPointer(instance_.get(), this);

	if (!config.fullscreen) {
		if (config.x.has_value() && config.y.has_value()) {
			glfwSetWindowPos(instance_.get(), *config.x, *config.y);
		} else {
			Center();
		}
	}

	if (config.minimized) {
		glfwIconifyWindow(instance_.get());
	}

	SetMouseMode(config.mouse_mode);

	glfwSetWindowSizeLimits(instance_.get(), 1, 1, GLFW_DONT_CARE, GLFW_DONT_CARE);

	PTGN_INFO("Created window with config: ", config);

	// Callbacks should be set after window setup so they dont trigger initially.
	SetCallbacks();

	ImGui_ImplGlfw_InitForOpenGL(instance_.get(), true);
#ifdef __EMSCRIPTEN__
	ImGui_ImplGlfw_InstallEmscriptenCallbacks(instance_.get(), "#canvas");
#endif
	ImGui_ImplOpenGL3_Init(glsl_version);
}

Window::~Window() {
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

void Window::SwapBuffers() const {
	glfwSwapBuffers(instance_.get());
}

bool Window::PollEvents() {
	PTGN_ASSERT(event_sink_, "Cannot poll window events before setting an event sink");

	glfwPollEvents();

	auto context{ glfwGetCurrentContext() };
	PTGN_ASSERT(context == instance_.get());
	PTGN_ASSERT(instance_.get() != nullptr);

	bool focused{ focused_ };

#ifdef __EMSCRIPTEN__
	// Emscripten does not support window focus, so we assume the window is always focused.
	focused = true;
#endif

	previous_mouse_position_ = mouse_position_;

	bool mouse_moved{ !raw_mouse_position_.IsZero() };

	auto half_window_size{ GetSize() / 2.0f };

	if (mouse_moved) {
		mouse_position_ = raw_mouse_position_ - half_window_size;
		auto delta{ mouse_position_ - previous_mouse_position_ };
		if (focused) {
			PushEvent<event::MouseMove>(mouse_position_, delta);
		}
		raw_mouse_position_ = {};
		mouse_set_			= true;
	} else if (focused) {
		double x{ 0.0 };
		double y{ 0.0 };
		glfwGetCursorPos(instance_.get(), &x, &y);

		auto new_mouse_position{ V2_float{ x, y } - half_window_size };

		auto delta{ new_mouse_position - mouse_position_ };

		if (!delta.IsZero()) {
			mouse_position_ = new_mouse_position;
			if (mouse_set_) {
				PushEvent<event::MouseMove>(mouse_position_, delta);
			}
			mouse_set_ = true;
		}
	}

	mouse_scroll_ = raw_scroll_accum_;
	if (!mouse_scroll_.IsZero()) {
		mouse_scroll_timestamp_ = glfwGetTime();
	}
	raw_scroll_accum_ = {};

	if (focused && !mouse_scroll_.IsZero()) {
		PushEvent<event::MouseScroll>(mouse_scroll_, mouse_position_);
	}

	for (std::size_t i = 0; i < mouse_states_.size(); ++i) {
		bool was_down = prev_mouse_down_[i];
		bool is_down  = mouse_down_[i];

		using enum impl::MouseState;
		if (!was_down && is_down) {
			mouse_states_[i]	 = Pressed;
			mouse_timestamps_[i] = glfwGetTime();
			if (focused) {
				PushEvent<event::MousePressed>(static_cast<Mouse>(i), mouse_position_);
				PushEvent<event::MouseHeld>(static_cast<Mouse>(i), mouse_position_);
			}
		} else if (was_down && is_down) {
			mouse_states_[i] = Held;
			if (focused) {
				PushEvent<event::MouseHeld>(static_cast<Mouse>(i), mouse_position_);
			}
		} else if (was_down && !is_down) {
			mouse_states_[i]	 = Released;
			mouse_timestamps_[i] = glfwGetTime();
			if (focused) {
				PushEvent<event::MouseReleased>(static_cast<Mouse>(i), mouse_position_);
			}
		} else {
			mouse_states_[i] = Idle;
		}
	}

	for (std::size_t i = 0; i < key_states_.size(); ++i) {
		bool was_down = prev_key_down_[i];
		bool is_down  = key_down_[i];

		using enum impl::KeyState;
		if (!was_down && is_down) {
			key_states_[i]	   = Pressed;
			key_timestamps_[i] = glfwGetTime();
			if (focused) {
				PushEvent<event::KeyPressed>(static_cast<Key>(i));
				PushEvent<event::KeyHeld>(static_cast<Key>(i));
			}
		} else if (was_down && is_down) {
			key_states_[i] = Held;
			if (focused) {
				PushEvent<event::KeyHeld>(static_cast<Key>(i));
			}
		} else if (was_down && !is_down) {
			key_states_[i]	   = Released;
			key_timestamps_[i] = glfwGetTime();
			if (focused) {
				PushEvent<event::KeyReleased>(static_cast<Key>(i));
			}
		} else {
			key_states_[i] = Idle;
		}
	}

	prev_mouse_down_ = mouse_down_;
	prev_key_down_	 = key_down_;

	return !quit_;
}

void Window::CacheWindowedRect() {
	auto win = instance_.get();
	if (!win) {
		return;
	}

	if (glfwGetWindowMonitor(win) != nullptr) {
		return; // fullscreen, do not overwrite cached windowed rect
	}

	glfwGetWindowPos(win, &windowed_pos_.x, &windowed_pos_.y);
	glfwGetWindowSize(win, &windowed_size_.x, &windowed_size_.y);
	windowed_was_maximized_ = glfwGetWindowAttrib(win, GLFW_MAXIMIZED) == GLFW_TRUE;
}

void Window::SetMouseMode(MouseMode mode) {
	using enum MouseMode;
	int glfw_mode = GLFW_CURSOR_NORMAL;

	switch (mode) {
		case Normal:   glfw_mode = GLFW_CURSOR_NORMAL; break;
		case Hidden:   glfw_mode = GLFW_CURSOR_HIDDEN; break;
		case Disabled: glfw_mode = GLFW_CURSOR_DISABLED; break;
	}

	glfwSetInputMode(instance_.get(), GLFW_CURSOR, glfw_mode);

	// Optional: raw mouse input for better FPS camera behavior
	if (glfwRawMouseMotionSupported()) {
		if (mode == Disabled) {
			glfwSetInputMode(instance_.get(), GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
		} else {
			glfwSetInputMode(instance_.get(), GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
		}
	}
}

void Window::SetAlwaysOnTop(bool on) {
	glfwSetWindowAttrib(instance_.get(), GLFW_FLOATING, on ? GLFW_TRUE : GLFW_FALSE);
}

void Window::SetOSCursor(const path& img_filepath, V2_int cursor_hotspot) {
	impl::Surface surface{ img_filepath };
	PTGN_ASSERT(!surface.IsEmpty(), "Failed to load cursor surface");

	GLFWimage image{};
	image.width	 = surface.GetSize().x;
	image.height = surface.GetSize().y;
	image.pixels = const_cast<std::uint8_t*>(surface.Data()); // NOSONAR

	auto cursor = glfwCreateCursor(&image, cursor_hotspot.x, cursor_hotspot.y);
	PTGN_ASSERT(cursor != nullptr, "glfwCreateCursor failed");

	custom_cursor_.reset(cursor);
	glfwSetCursor(instance_.get(), custom_cursor_.get());
}

void Window::ResetOSCursor() {
	custom_cursor_.reset();
	glfwSetCursor(instance_.get(), nullptr);
}

void Window::SetOSCursorVisibility(bool visibility) {
	glfwSetInputMode(
		instance_.get(), GLFW_CURSOR, visibility ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_HIDDEN
	);
}

void Window::SetMinimumSize(V2_int minimum_size) {
	minimum_size_ = minimum_size;
	glfwSetWindowSizeLimits(
		instance_.get(), minimum_size_.x, minimum_size_.y, maximum_size_.x, maximum_size_.y
	);
}

V2_int Window::GetMinimumSize() const {
	return minimum_size_;
}

void Window::SetMaximumSize(V2_int maximum_size) {
	maximum_size_ = maximum_size;
	glfwSetWindowSizeLimits(
		instance_.get(), minimum_size_.x, minimum_size_.y, maximum_size_.x, maximum_size_.y
	);
}

V2_int Window::GetMaximumSize() const {
	return maximum_size_;
}

void Window::SetPosition(V2_int new_origin) {
	auto win = instance_.get();
	PTGN_ASSERT(win != nullptr, "Window is null");

	if (glfwGetWindowMonitor(win) == nullptr) {
		glfwSetWindowPos(win, new_origin.x, new_origin.y);
	}
}

V2_int Window::GetPosition() const {
	V2_int pos;
	glfwGetWindowPos(instance_.get(), &pos.x, &pos.y);
	return pos;
}

void Window::SetSize(V2_int new_size, bool centered) {
	auto win = instance_.get();
	PTGN_ASSERT(win != nullptr, "Window is null");

	glfwSetWindowSize(win, new_size.x, new_size.y);

	if (centered && glfwGetWindowMonitor(win) == nullptr) {
		Center();
	}
}

V2_int Window::GetSize() const {
	V2_int window_size;
	// glfwGetWindowSize(instance_.get(), &window_size.x, &window_size.y);
	glfwGetFramebufferSize(instance_.get(), &window_size.x, &window_size.y);
	return window_size;
}

void Window::SetBackgroundColor(Color background_color) {
	background_color_ = background_color;
}

Color Window::GetBackgroundColor() const {
	return background_color_;
}

void Window::Center() {
	auto monitor = glfwGetPrimaryMonitor();
	PTGN_ASSERT(monitor != nullptr, "glfwGetPrimaryMonitor failed");

	V2_int monitor_pos;
	V2_int monitor_size;
	glfwGetMonitorWorkarea(
		monitor, &monitor_pos.x, &monitor_pos.y, &monitor_size.x, &monitor_size.y
	);

	V2_int window_size;
	glfwGetWindowSize(instance_.get(), &window_size.x, &window_size.y);

	auto window_pos{ monitor_pos + (monitor_size - window_size) / 2 };

	glfwSetWindowPos(instance_.get(), window_pos.x, window_pos.y);
}

void Window::SetTitle(std::string_view title) {
	title_ = title;
	glfwSetWindowTitle(instance_.get(), title_.c_str());
}

std::string_view Window::GetTitle() const {
	return title_;
}

void Window::SetSetting(WindowSetting setting) {
	auto win = instance_.get();
	PTGN_ASSERT(win != nullptr, "Window is null");

	switch (setting) {
		using enum WindowSetting;
		case None:		 break;
		case Shown:		 glfwShowWindow(win); break;
		case Hidden:	 glfwHideWindow(win); break;
		case Windowed:	 SetFullscreen(false); break;
		case Fullscreen: SetFullscreen(true); break;
		case Borderless: glfwSetWindowAttrib(win, GLFW_DECORATED, GLFW_FALSE); break;
		case Bordered:	 glfwSetWindowAttrib(win, GLFW_DECORATED, GLFW_TRUE); break;
		case Resizable:	 glfwSetWindowAttrib(win, GLFW_RESIZABLE, GLFW_TRUE); break;
		case FixedSize:	 glfwSetWindowAttrib(win, GLFW_RESIZABLE, GLFW_FALSE); break;
		case Minimized:	 glfwIconifyWindow(win); break;
		case Maximized:
			if (glfwGetWindowMonitor(win) == nullptr) {
				glfwMaximizeWindow(win);
				windowed_was_maximized_ = true;
			}
			break;
		default: PTGN_ERROR("Unknown WindowSetting: ", std::to_underlying(setting));
	}
}

bool Window::GetSetting(WindowSetting setting) const {
	auto win = instance_.get();
	PTGN_ASSERT(win != nullptr, "Window is null");

	switch (setting) {
		using enum WindowSetting;
		case None:		 return false;
		case Shown:		 return glfwGetWindowAttrib(win, GLFW_VISIBLE) == GLFW_TRUE;
		case Hidden:	 return glfwGetWindowAttrib(win, GLFW_VISIBLE) == GLFW_FALSE;
		case Windowed:	 return glfwGetWindowMonitor(win) == nullptr;
		case Fullscreen: return glfwGetWindowMonitor(win) != nullptr;
		case Borderless: return glfwGetWindowAttrib(win, GLFW_DECORATED) == GLFW_FALSE;
		case Bordered:	 return glfwGetWindowAttrib(win, GLFW_DECORATED) == GLFW_TRUE;
		case Resizable:	 return glfwGetWindowAttrib(win, GLFW_RESIZABLE) == GLFW_TRUE;
		case FixedSize:	 return glfwGetWindowAttrib(win, GLFW_RESIZABLE) == GLFW_FALSE;
		case Maximized:	 return glfwGetWindowAttrib(win, GLFW_MAXIMIZED) == GLFW_TRUE;
		case Minimized:	 return glfwGetWindowAttrib(win, GLFW_ICONIFIED) == GLFW_TRUE;
		default:		 PTGN_ERROR("Unknown WindowSetting: ", std::to_underlying(setting));
	}
}

void Window::SetResizable() {
	SetSetting(WindowSetting::Resizable);
}

void Window::SetFixedSize() {
	SetSetting(WindowSetting::FixedSize);
}

void Window::SetFullscreen(bool on) {
	auto win = instance_.get();
	PTGN_ASSERT(win != nullptr, "Window is null");

	if (bool is_fullscreen = (glfwGetWindowMonitor(win) != nullptr); on == is_fullscreen) {
		return;
	}

	if (on) {
		CacheWindowedRect();

		auto monitor = glfwGetPrimaryMonitor();
		PTGN_ASSERT(monitor != nullptr, "glfwGetPrimaryMonitor failed");

		const auto mode = glfwGetVideoMode(monitor);
		PTGN_ASSERT(mode != nullptr, "glfwGetVideoMode failed");

		glfwSetWindowMonitor(win, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
	} else {
		glfwSetWindowMonitor(
			win, nullptr, windowed_pos_.x, windowed_pos_.y, windowed_size_.x, windowed_size_.y,
			GLFW_DONT_CARE
		);

		if (windowed_was_maximized_) {
			glfwMaximizeWindow(win);
		}
	}
}

static duration<double> GetTimeSince(impl::Timestamp timestamp) {
	return duration<double>{ glfwGetTime() - timestamp };
}

V2_float Window::GetMousePosition() const {
	return mouse_position_;
}

V2_float Window::GetPreviousMousePosition() const {
	return previous_mouse_position_;
}

V2_float Window::GetMouseDelta() const {
	return GetMousePosition() - GetPreviousMousePosition();
}

V2_float Window::GetMouseScroll() const {
	return mouse_scroll_;
}

bool Window::MousePressed(Mouse mouse_button) const {
	return mouse_states_[std::to_underlying(mouse_button)] == impl::MouseState::Pressed;
}

bool Window::MouseReleased(Mouse mouse_button) const {
	return mouse_states_[std::to_underlying(mouse_button)] == impl::MouseState::Released;
}

bool Window::MouseHeld(Mouse mouse_button) const {
	auto state{ mouse_states_[std::to_underlying(mouse_button)] };
	return state == impl::MouseState::Held || state == impl::MouseState::Pressed;
}

bool Window::MouseHeld(Mouse mouse_button, milliseconds time) const {
	return GetMouseHeldTime(mouse_button) >= time;
}

milliseconds Window::GetMouseHeldTime(Mouse button) const {
	return duration_cast<milliseconds>(GetTimeSince(mouse_timestamps_[std::to_underlying(button)]));
}

bool Window::KeyPressed(Key key) const {
	return key_states_[std::to_underlying(key)] == impl::KeyState::Pressed;
}

bool Window::KeyReleased(Key key) const {
	return key_states_[std::to_underlying(key)] == impl::KeyState::Released;
}

bool Window::KeyHeld(Key key) const {
	auto state{ key_states_[std::to_underlying(key)] };
	return state == impl::KeyState::Held || state == impl::KeyState::Pressed;
}

bool Window::KeyHeld(Key key, milliseconds time) const {
	return GetKeyHeldTime(key) >= time;
}

milliseconds Window::GetKeyHeldTime(Key key) const {
	return duration_cast<milliseconds>(GetTimeSince(key_timestamps_[std::to_underlying(key)]));
}

void Window::ClearInputState() {
	for (std::size_t i = 0; i < key_states_.size(); ++i) {
		key_down_[i] = false;
	}

	for (std::size_t i = 0; i < mouse_states_.size(); ++i) {
		mouse_down_[i] = false;
	}

	raw_mouse_position_ = {};
	raw_scroll_accum_	= {};
}

} // namespace ptgn