#include "core/app/window.h"

#include <memory>
#include <string>
#include <string_view>

#include "SDL_error.h"
#include "SDL_mouse.h"
#include "SDL_stdinc.h"
#include "SDL_video.h"
#include "debug/core/log.h"
#include "debug/runtime/assert.h"
#include "math/vector2.h"

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

void Window::SetCanvasSize(const V2_int& new_size) const {
	emscripten_set_element_css_size("#canvas", new_size.x, new_size.y);
}

V2_int Window::GetCanvasSize() const {
	return { get_canvas_width(), get_canvas_height() };
}

#endif

namespace impl {

void WindowDeleter::operator()(SDL_Window* window) const {
	SDL_DestroyWindow(window);
	PTGN_INFO("Destroyed SDL2 window");
}

} // namespace impl

V2_int Screen::GetSize() {
	SDL_DisplayMode dm;
	if (SDL_GetDesktopDisplayMode(0, &dm) != 0) {
		PTGN_LOG("SDL_GetDesktopDisplayMode failed: %s", SDL_GetError());
		return {};
	}
	return { dm.w, dm.h };
}

Window::Window(const char* title, const V2_int& size) :
	// TODO: Add flags to window constructor.
	instance_{ SDL_CreateWindow(
				   title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, size.x, size.y,
				   SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE
			   ),
			   impl::WindowDeleter{} } {
	PTGN_ASSERT(instance_, "SDL_CreateWindow failed: {}", SDL_GetError());
	PTGN_INFO("Created SDL2 window");
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
	SDL_SetRelativeMouseMode(static_cast<SDL_bool>(on));
}

void Window::SetMouseGrab(bool on) const {
	SDL_SetWindowMouseGrab(*this, static_cast<SDL_bool>(on));
}

void Window::CaptureMouse(bool on) const {
	SDL_CaptureMouse(static_cast<SDL_bool>(on));
}

void Window::SetAlwaysOnTop(bool on) const {
	SDL_SetWindowAlwaysOnTop(*this, static_cast<SDL_bool>(on));
}

void Window::SetMinimumSize(const V2_int& minimum_size) const {
	SDL_SetWindowMinimumSize(*this, minimum_size.x, minimum_size.y);
}

V2_int Window::GetMinimumSize() const {
	V2_int minimum_size;
	SDL_GetWindowMinimumSize(*this, &minimum_size.x, &minimum_size.y);
	return minimum_size;
}

void Window::SetMaximumSize(const V2_int& maximum_size) const {
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

void Window::SetSize(const V2_int& new_size, bool centered) const {
#ifdef __EMSCRIPTEN__
	SetCanvasSize(new_size);
#endif
	SDL_SetWindowSize(*this, new_size.x, new_size.y);
	// Important to center after resizing.
	if (centered) {
		Center();
	}
}

void Window::SetPosition(const V2_int& new_origin) const {
	SDL_SetWindowPosition(*this, new_origin.x, new_origin.y);
}

void Window::Center() const {
	SetPosition(V2_int{ SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED });
}

void Window::SetTitle(const std::string& new_title) const {
	return SDL_SetWindowTitle(*this, new_title.c_str());
}

void Window::SetSetting(WindowSetting setting) const {
	SDL_Window* win{ *this };
	switch (setting) {
		using enum ptgn::WindowSetting;
		case Shown:		 SDL_ShowWindow(win); break;
		case Hidden:	 SDL_HideWindow(win); break;
		case Windowed:	 SDL_SetWindowFullscreen(win, 0); break;
		case Fullscreen: SDL_SetWindowFullscreen(win, SDL_WINDOW_FULLSCREEN_DESKTOP); break;
		case Borderless: SDL_SetWindowBordered(win, SDL_FALSE); break;
		case Bordered:	 SDL_SetWindowBordered(win, SDL_TRUE); break;
		case Resizable:	 SDL_SetWindowResizable(win, SDL_TRUE); break;
		case FixedSize:	 SDL_SetWindowResizable(win, SDL_FALSE); break;
		case Maximized:
			SDL_SetWindowResizable(win, SDL_TRUE);
			SDL_MaximizeWindow(win);
			break;
		case Minimized: SDL_MinimizeWindow(win); break;
		default:		PTGN_ERROR("Cannot set unrecognized window setting");
	}
}

bool Window::GetSetting(WindowSetting setting) const {
	std::uint32_t flags{ SDL_GetWindowFlags(*this) };
	switch (setting) {
		using enum ptgn::WindowSetting;
		case Shown:	   return flags & SDL_WINDOW_SHOWN;
		case Hidden:   return !(flags & SDL_WINDOW_SHOWN);
		case Windowed: return !(flags & (SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_FULLSCREEN));
		case Fullscreen:
			return (flags & (SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_FULLSCREEN)) ==
				   SDL_WINDOW_FULLSCREEN_DESKTOP;
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