#include "platform/window/window.h"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>

#include <ios>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

#include "core/assert.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "renderer/primitives/color.h"

#ifdef __EMSCRIPTEN__

#include <emscripten.h>
#include <emscripten/html5.h>

#include <cstdint>
#include <functional>

EM_JS(int, get_canvas_width, (), { return Module.canvas.width; });
EM_JS(int, get_canvas_height, (), { return Module.canvas.height; });

#endif

namespace ptgn {

#ifdef __EMSCRIPTEN__

void Window::SetCanvasSize(V2_int new_size) const {
	emscripten_set_element_css_size("#canvas", new_size.x, new_size.y);
}

V2_int Window::GetCanvasSize() const {
	return { get_canvas_width(), get_canvas_height() };
}

#endif

namespace impl {

void WindowDeleter::operator()(SDL_Window* window) const {
	SDL_DestroyWindow(window);
	PTGN_INFO("Destroyed window");
}

} // namespace impl

Window::Window(const WindowConfig& config) {
	PTGN_ASSERT(
		config.minimized || config.maximized || config.fullscreen ||
			!config.minimized && !config.maximized && !config.fullscreen,
		"Window config can only be one of fullscreen, minimized or maximized at once"
	);
	SDL_PropertiesID props = SDL_CreateProperties();
	SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, config.title.c_str());
	SDL_SetNumberProperty(
		props, SDL_PROP_WINDOW_CREATE_X_NUMBER, config.x.value_or(SDL_WINDOWPOS_CENTERED)
	);
	SDL_SetNumberProperty(
		props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, config.y.value_or(SDL_WINDOWPOS_CENTERED)
	);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, config.size.x);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, config.size.y);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, config.resizeable);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_TRANSPARENT_BOOLEAN, config.transparent);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_MAXIMIZED_BOOLEAN, config.maximized);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_MINIMIZED_BOOLEAN, config.minimized);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, config.fullscreen);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_BORDERLESS_BOOLEAN, config.borderless);
	SDL_SetNumberProperty(
		props, SDL_PROP_WINDOW_CREATE_ALWAYS_ON_TOP_BOOLEAN, config.always_on_top
	);
	SDL_SetNumberProperty(
		props, SDL_PROP_WINDOW_CREATE_MOUSE_GRABBED_BOOLEAN, config.mouse_grabbed
	);

	SDL_SetNumberProperty(
		props, SDL_PROP_WINDOW_CREATE_FLAGS_NUMBER, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN
	);

	instance_ = { SDL_CreateWindowWithProperties(props), impl::WindowDeleter{} };

	SDL_SetWindowMinimumSize(instance_.get(), 1, 1);

	PTGN_ASSERT(instance_, "SDL_CreateWindow failed: {}", SDL_GetError());
	PTGN_INFO("Created window with config: ", config);
}

Window::operator SDL_Window*() const {
	PTGN_ASSERT(instance_ != nullptr, "Window uninitialized or destroyed");
	return instance_.get();
}

void Window::SwapBuffers() const {
	SDL_GL_SwapWindow(*this);
}

V2_int Window::GetSize() const {
	V2_int size;
	SDL_GetWindowSizeInPixels(*this, &size.x, &size.y);
	return size;
}

void Window::SetRelativeMouseMode(bool on) const {
	SDL_SetWindowRelativeMouseMode(*this, on);
}

void Window::SetMouseGrab(bool on) const {
	SDL_SetWindowMouseGrab(*this, on);
}

void Window::CaptureMouse(bool on) const {
	SDL_CaptureMouse(on);
}

void Window::SetAlwaysOnTop(bool on) const {
	SDL_SetWindowAlwaysOnTop(*this, on);
}

void Window::SetMinimumSize(V2_int minimum_size) const {
	SDL_SetWindowMinimumSize(*this, minimum_size.x, minimum_size.y);
}

V2_int Window::GetMinimumSize() const {
	V2_int minimum_size;
	SDL_GetWindowMinimumSize(*this, &minimum_size.x, &minimum_size.y);
	return minimum_size;
}

void Window::SetMaximumSize(V2_int maximum_size) const {
	SDL_SetWindowMaximumSize(*this, maximum_size.x, maximum_size.y);
}

V2_int Window::GetMaximumSize() const {
	V2_int maximum_size;
	SDL_GetWindowMinimumSize(*this, &maximum_size.x, &maximum_size.y);
	return maximum_size;
}

V2_int Window::GetPosition() const {
	V2_int origin;
	SDL_GetWindowPosition(*this, &origin.x, &origin.y);
	return origin;
}

std::string_view Window::GetTitle() const {
	return SDL_GetWindowTitle(*this);
}

void Window::SetSize(V2_int new_size, bool centered) const {
#ifdef __EMSCRIPTEN__
	SetCanvasSize(new_size);
#endif
	SDL_SetWindowSize(*this, new_size.x, new_size.y);
	// Important to center after resizing.
	if (centered) {
		Center();
	}
}

void Window::SetBackgroundColor(Color background_color) {
	background_color_ = background_color;
}

Color Window::GetBackgroundColor() const {
	return background_color_;
}

void Window::SetPosition(V2_int new_origin) const {
	SDL_SetWindowPosition(*this, new_origin.x, new_origin.y);
}

void Window::Center() const {
	SetPosition(V2_int{ SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED });
}

void Window::SetTitle(const std::string& new_title) const {
	bool set{ SDL_SetWindowTitle(*this, new_title.c_str()) };
	PTGN_ASSERT(set, SDL_GetError());
}

void Window::SetSetting(WindowSetting setting) const {
	SDL_Window* win{ *this };
	switch (setting) {
		using enum WindowSetting;
		case Shown:		 SDL_ShowWindow(win); break;
		case Hidden:	 SDL_HideWindow(win); break;
		case Windowed:	 SDL_SetWindowFullscreen(win, false); break;
		case Fullscreen: SDL_SetWindowFullscreen(win, true); break;
		case Borderless: SDL_SetWindowBordered(win, false); break;
		case Bordered:	 SDL_SetWindowBordered(win, true); break;
		case Resizable:	 SDL_SetWindowResizable(win, true); break;
		case FixedSize:	 SDL_SetWindowResizable(win, false); break;
		case Maximized:
			SDL_SetWindowResizable(win, true);
			SDL_MaximizeWindow(win);
			break;
		case Minimized: SDL_MinimizeWindow(win); break;
		default:		PTGN_ERROR("Cannot set unrecognized window setting");
	}
}

bool Window::GetSetting(WindowSetting setting) const {
	SDL_WindowFlags flags{ SDL_GetWindowFlags(*this) };
	switch (setting) {
		using enum ptgn::WindowSetting;
		case Shown:		 PTGN_ERROR("Cannot query this setting: not migrated to SDL3 yet"); break;
		case Windowed:	 PTGN_ERROR("Cannot query this setting: not migrated to SDL3 yet"); break;
		case Fullscreen: PTGN_ERROR("Cannot query this setting: not migrated to SDL3 yet"); break;
		case Hidden:	 return flags & SDL_WINDOW_HIDDEN;
		case Borderless: return flags & SDL_WINDOW_BORDERLESS;
		case Bordered:	 return !(flags & SDL_WINDOW_BORDERLESS);
		case Resizable:	 return flags & SDL_WINDOW_RESIZABLE;
		case FixedSize:	 return !(flags & SDL_WINDOW_RESIZABLE);
		case Maximized:	 return flags & SDL_WINDOW_MAXIMIZED;
		case Minimized:	 return flags & SDL_WINDOW_MINIMIZED;
		default:		 PTGN_ERROR("Cannot retrieve unrecognized window setting");
	}
}

void Window::SetResizable() const {
	SetSetting(WindowSetting::Resizable);
}

void Window::SetFixedSize() const {
	SetSetting(WindowSetting::FixedSize);
}

void Window::SetFullscreen() const {
	SetSetting(WindowSetting::Fullscreen);
}

std::ostream& operator<<(std::ostream& os, const WindowConfig& config) {
	os << std::boolalpha;
	auto x = config.x.has_value() ? std::to_string(*config.x) : "centered";
	auto y = config.y.has_value() ? std::to_string(*config.y) : "centered";
	os << "{\n"
	   << "  title: \"" << config.title << "\",\n"
	   << "  size: " << config.size << ",\n"
	   << "  resizeable: " << config.resizeable << ",\n"
	   << "  position: (" << x << ", " << y << "),\n"
	   << "  minimized: " << config.minimized << ",\n"
	   << "  maximized: " << config.maximized << ",\n"
	   << "  fullscreen: " << config.fullscreen << ",\n"
	   << "  mouse_grabbed: " << config.mouse_grabbed << ",\n"
	   << "  always_on_top: " << config.always_on_top << ",\n"
	   << "  borderless: " << config.borderless << ",\n"
	   << "  transparent: " << config.transparent << "\n"
	   << "}";
	os << std::noboolalpha;

	return os;
}

} // namespace ptgn