#include "platform/window.h"

#ifdef __EMSCRIPTEN__

#include <emscripten.h>
#include <emscripten/html5.h>

EM_JS(int, get_canvas_width, (), { return Module.canvas.width; });
EM_JS(int, get_canvas_height, (), { return Module.canvas.height; });

#endif

#include <chrono>
#include <cstdint>
#include <ios>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

#include "core/assert.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "core/util/file.h"
#include "platform/events.h"
#include "platform/glfw.h"
#include "platform/key.h"
#include "platform/mouse.h"
#include "renderer/backend/gl/gl.h"
#include "renderer/image/surface.h"
#include "renderer/primitives/color.h"
#include "renderer/renderer.h"
#include "runtime/event/event_handler.h"

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
		self->events_.Push<event::WindowMoved>(pos);
	});

	glfwSetWindowMaximizeCallback(win, [](GLFWwindow* window, int maximized) {
		auto self{ static_cast<Window*>(glfwGetWindowUserPointer(window)) };
		if (!self) {
			return;
		}
		if (maximized) {
			self->events_.Push<event::WindowMaximized>();
		}
	});

	glfwSetWindowIconifyCallback(win, [](GLFWwindow* window, int iconified) {
		auto self{ static_cast<Window*>(glfwGetWindowUserPointer(window)) };
		if (!self) {
			return;
		}
		if (iconified) {
			self->events_.Push<event::WindowMinimized>();
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

		self->renderer_.OnWindowResize(size);
		self->events_.Push<event::WindowResized>(size);
	});

	glfwSetWindowFocusCallback(win, [](GLFWwindow* window, int focused) {
		auto self{ static_cast<Window*>(glfwGetWindowUserPointer(window)) };
		if (!self) {
			return;
		}
		if (focused) {
			self->focused_ = true;
			self->events_.Push<event::WindowFocusGained>();
		} else {
			self->ClearInputState();
			self->focused_ = false;
			self->events_.Push<event::WindowFocusLost>();
		}
	});

	glfwSetWindowCloseCallback(win, [](GLFWwindow* window) {
		auto self{ static_cast<Window*>(glfwGetWindowUserPointer(window)) };
		if (!self) {
			return;
		}

		self->quit_ = true;
		self->events_.Push<event::WindowQuit>();
	});

	glfwSetKeyCallback(win, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
		auto self{ static_cast<Window*>(glfwGetWindowUserPointer(window)) };
		if (!self || key < 0) {
			return;
		}

		PTGN_ASSERT(key < self->key_states_.size(), "Key outside of range of valid keys");

		switch (action) {
			case GLFW_PRESS: {
				self->key_timestamps_[key] = glfwGetTime();
				self->key_states_[key]	   = impl::KeyState::Pressed;
				self->events_.Push<event::KeyPressed>(Key{ key });
				self->events_.Push<event::KeyHeld>(Key{ key });
				break;
			}
			case GLFW_RELEASE: {
				self->key_timestamps_[key] = glfwGetTime();
				self->key_states_[key]	   = impl::KeyState::Released;
				self->events_.Push<event::KeyReleased>(Key{ key });
				break;
			}
			case GLFW_REPEAT: {
				self->key_states_[key] = impl::KeyState::Held;
				self->events_.Push<event::KeyPressed>(Key{ key });
				self->events_.Push<event::KeyHeld>(Key{ key });
			}
		}
	});

	// TODO: In the future add unicode callback.
	// glfwSetCharCallback(win, [](GLFWwindow* window, unsigned int keycode) {
	//	auto self{ static_cast<Window*>(glfwGetWindowUserPointer(window)) };
	//	if (!self) {
	//		return;
	//	}
	//});

	glfwSetMouseButtonCallback(win, [](GLFWwindow* window, int button, int action, int mods) {
		auto self{ static_cast<Window*>(glfwGetWindowUserPointer(window)) };
		if (!self || button < 0) {
			return;
		}

		PTGN_ASSERT(
			button < self->mouse_states_.size(),
			"Mouse button outside of range of valid mouse buttons"
		);

		switch (action) {
			case GLFW_PRESS: {
				self->mouse_timestamps_[button] = glfwGetTime();
				self->mouse_states_[button]		= impl::MouseState::Pressed;
				self->events_.Push<event::MousePressed>(Mouse{ button }, self->GetMousePosition());
				self->events_.Push<event::MouseHeld>(Mouse{ button }, self->GetMousePosition());
				break;
			}
			case GLFW_RELEASE: {
				self->mouse_timestamps_[button] = glfwGetTime();
				self->mouse_states_[button]		= impl::MouseState::Released;
				self->events_.Push<event::MouseReleased>(Mouse{ button }, self->GetMousePosition());
				break;
			}
		}
	});

	glfwSetScrollCallback(win, [](GLFWwindow* window, double x, double y) {
		auto self{ static_cast<Window*>(glfwGetWindowUserPointer(window)) };
		if (!self) {
			return;
		}

		self->mouse_scroll_timestamp_  = glfwGetTime();
		self->mouse_scroll_			   = V2_float{ x, y };
		self->mouse_scroll_delta_	  += self->mouse_scroll_;

		self->events_.Push<event::MouseScroll>(self->mouse_scroll_, self->GetMousePosition());
	});

	glfwSetCursorPosCallback(win, [](GLFWwindow* window, double x, double y) {
		auto self{ static_cast<Window*>(glfwGetWindowUserPointer(window)) };
		if (!self) {
			return;
		}

		V2_float pos{ x, y };

		auto half_window_size{ self->GetSize() / 2.0f };

		self->mouse_position_ = pos - half_window_size;

		self->events_.Push<event::MouseMove>(
			self->mouse_position_, self->previous_mouse_position_ - self->mouse_position_
		);
	});
}

Window::Window(EventHandler& events, Renderer& renderer, const WindowConfig& config) :
	title_{ config.title }, events_{ events }, renderer_{ renderer } {
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

#ifndef __EMSCRIPTEN__
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
#endif

	instance_ = std::unique_ptr<GLFWwindow, impl::WindowDeleter>{
		glfwCreateWindow(config.size.x, config.size.y, title_.c_str(), monitor, nullptr),
		impl::WindowDeleter{}
	};

	PTGN_ASSERT(instance_ != nullptr, "glfwCreateWindow failed");

	glfwMakeContextCurrent(instance_.get());

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
}

void Window::SwapBuffers() const {
	glfwSwapBuffers(instance_.get());
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

		case Maximized:
			if (glfwGetWindowMonitor(win) == nullptr) {
				glfwMaximizeWindow(win);
				windowed_was_maximized_ = true;
			}
			break;

		case Minimized: glfwIconifyWindow(win); break;
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
		default:		 return false;
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

std::ostream& operator<<(std::ostream& os, const MouseMode& mode) {
	switch (mode) {
		using enum MouseMode;
		case Normal:   return os << "Normal";
		case Hidden:   return os << "Hidden";
		case Disabled: return os << "Disabled";
		default:	   PTGN_ERROR("Unknown MouseMode: ", std::to_underlying(mode));
	}
}

std::ostream& operator<<(std::ostream& os, const WindowConfig& config) {
	os << std::boolalpha;
	auto x = config.x.has_value() ? std::to_string(*config.x) : "centered";
	auto y = config.y.has_value() ? std::to_string(*config.y) : "centered";
	os << "{\n"
	   << "  title: \"" << config.title << "\",\n"
	   << "  size: " << config.size << ",\n"
	   << "  resizable: " << config.resizable << ",\n"
	   << "  position: (" << x << ", " << y << "),\n"
	   << "  minimized: " << config.minimized << ",\n"
	   << "  maximized: " << config.maximized << ",\n"
	   << "  fullscreen: " << config.fullscreen << ",\n"
	   << "  mouse_mode: " << config.mouse_mode << ",\n"
	   << "  always_on_top: " << config.always_on_top << ",\n"
	   << "  borderless: " << config.borderless << ",\n"
	   << "  transparent: " << config.transparent << "\n"
	   << "}";
	os << std::noboolalpha;

	return os;
}

bool Window::Update() {
	previous_mouse_position_ = mouse_position_;
	mouse_scroll_			 = {};
	mouse_scroll_delta_		 = {};

	// Set key state from pressed to held and from released to idle to ensure those states only
	// last one frame.
	for (std::size_t i{ 0 }; i < key_states_.size(); ++i) {
		using enum impl::KeyState;
		auto& state{ key_states_[i] };
		if (state == Released) {
			state			   = Idle;
			key_timestamps_[i] = glfwGetTime();
		} else if (state == Pressed) {
			state = Held;
		}
	}

	// Set mouse button states from pressed to held and from released to idle to ensure those states
	// only last one frame.
	for (std::size_t i{ 0 }; i < mouse_states_.size(); ++i) {
		using enum impl::MouseState;
		auto& state{ mouse_states_[i] };
		if (state == Released) {
			state				 = Idle;
			mouse_timestamps_[i] = glfwGetTime();
		} else if (state == Pressed) {
			state = Held;
		}
	}

	glfwPollEvents();

	auto context{ glfwGetCurrentContext() };
	PTGN_ASSERT(context == instance_.get());
	PTGN_ASSERT(instance_.get() != nullptr);

	if (focused_) {
		// Before polling events, their states are updated from pressed to held and from released to
		// idle. This means that if a key or mouse button is still held after polling events, it has
		// must have been held.
		for (std::size_t i{ 0 }; i < mouse_states_.size(); ++i) {
			if (mouse_states_[i] == impl::MouseState::Held) {
				events_.Push<event::MouseHeld>(static_cast<Mouse>(i), mouse_position_);
			}
		}

		for (std::size_t i{ 0 }; i < key_states_.size(); ++i) {
			if (key_states_[i] == impl::KeyState::Held) {
				events_.Push<event::KeyHeld>(static_cast<Key>(i));
			}
		}
	}

	return !quit_;
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

float Window::GetMouseScroll() const {
	return mouse_scroll_delta_.y;
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
		key_states_[i]	   = impl::KeyState::Idle;
		key_timestamps_[i] = glfwGetTime();
	}

	for (std::size_t i = 0; i < mouse_states_.size(); ++i) {
		mouse_states_[i]	 = impl::MouseState::Idle;
		mouse_timestamps_[i] = glfwGetTime();
	}

	mouse_scroll_		= {};
	mouse_scroll_delta_ = {};
}

} // namespace ptgn