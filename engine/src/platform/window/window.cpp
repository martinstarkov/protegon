#include "platform/window/window.h"

#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>

#include <memory>
#include <string>
#include <string_view>

#include "core/assert.h"
#include "core/log.h"
#include "core/math/vector2.h"

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

Window::Window(const char* title, V2_int size) {
	// TODO: Add flags to window constructor.
	SDL_PropertiesID props = SDL_CreateProperties();
	SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, title);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, SDL_WINDOWPOS_CENTERED);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, SDL_WINDOWPOS_CENTERED);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, size.x);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, size.y);
	SDL_SetNumberProperty(
		props, SDL_PROP_WINDOW_CREATE_FLAGS_NUMBER,
		SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE
	);

	instance_ = { SDL_CreateWindowWithProperties(props), impl::WindowDeleter{} };

	PTGN_ASSERT(instance_, "SDL_CreateWindow failed: {}", SDL_GetError());
	PTGN_INFO("Created window");
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
		using enum ptgn::WindowSetting;
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

} // namespace ptgn