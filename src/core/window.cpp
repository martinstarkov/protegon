#include "core/window.h"

#include <memory>
#include <string>
#include <string_view>

#include "common/assert.h"
#include "core/game.h"
#include "core/sdl_instance.h"
#include "debug/log.h"
#include "math/vector2.h"
#include "SDL_error.h"
#include "SDL_mouse.h"
#include "SDL_stdinc.h"
#include "SDL_video.h"

#ifdef __EMSCRIPTEN__

#include <emscripten.h>
#include <emscripten/html5.h>

EM_JS(int, get_canvas_width, (), { return Module.canvas.width; });
EM_JS(int, get_canvas_height, (), { return Module.canvas.height; });

#endif

namespace ptgn {

V2_int Screen::GetSize() {
	SDL_DisplayMode dm;
	if (SDL_GetDesktopDisplayMode(0, &dm) != 0) {
		PTGN_LOG("SDL_GetDesktopDisplayMode failed: %s", SDL_GetError());
		return {};
	}
	return { dm.w, dm.h };
}

namespace impl {

#ifdef __EMSCRIPTEN__

void Window::SetCanvasSize(const V2_int& new_size) const {
	emscripten_set_element_css_size("#canvas", new_size.x, new_size.y);
}

V2_int Window::GetCanvasSize() const {
	return { get_canvas_width(), get_canvas_height() };
}

#endif

void WindowDeleter::operator()(SDL_Window* window) const {
	if (game.sdl_instance_->SDLIsInitialized()) {
		SDL_DestroyWindow(window);
		PTGN_INFO("Destroyed SDL2 window");
	}
}

WindowInstance::WindowInstance() {
	// Windows start zero-sized.
	window_.reset(SDL_CreateWindow(
		"", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 0, 0,
		SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE
	));
}

WindowInstance::operator SDL_Window*() const {
	PTGN_ASSERT(window_ != nullptr, "Window uninitialized or destroyed");
	return window_.get();
}

void Window::Init() {
	PTGN_ASSERT(!IsValid(), "Previous window must be destroyed before initializing a new one");
	Create();
	PTGN_ASSERT(IsValid(), SDL_GetError());
	PTGN_INFO("Created SDL2 window");
}

void Window::Shutdown() {
	Destroy();
}

void* Window::CreateGLContext() {
	return SDL_GL_CreateContext(Get());
}

int Window::MakeGLContextCurrent(void* context) {
	return SDL_GL_MakeCurrent(Get(), context);
}

void Window::SwapBuffers() const {
	SDL_GL_SwapWindow(Get());
}

V2_int Window::GetSize() const {
	PTGN_ASSERT(IsValid(), "Cannot get size of uninitialized or destroyed window");
	V2_int size;
	SDL_GetWindowSizeInPixels(Get(), &size.x, &size.y);
	return size;
}

V2_float Window::GetCenter() const {
	return GetSize() / 2.0f;
}

void Window::SetRelativeMouseMode(bool on) const {
	SDL_SetRelativeMouseMode(static_cast<SDL_bool>(on));
}

void Window::SetMouseGrab(bool on) const {
	SDL_SetWindowMouseGrab(Get(), static_cast<SDL_bool>(on));
}

void Window::CaptureMouse(bool on) const {
	SDL_CaptureMouse(static_cast<SDL_bool>(on));
}

void Window::SetAlwaysOnTop(bool on) const {
	SDL_SetWindowAlwaysOnTop(Get(), static_cast<SDL_bool>(on));
}

void Window::SetMinimumSize(const V2_int& minimum_size) const {
	SDL_SetWindowMinimumSize(Get(), minimum_size.x, minimum_size.y);
}

V2_int Window::GetMinimumSize() const {
	V2_int minimum_size;
	SDL_GetWindowMinimumSize(Get(), &minimum_size.x, &minimum_size.y);
	return minimum_size;
}

void Window::SetMaximumSize(const V2_int& maximum_size) const {
	SDL_SetWindowMaximumSize(Get(), maximum_size.x, maximum_size.y);
}

V2_int Window::GetMaximumSize() const {
	V2_int maximum_size;
	SDL_GetWindowMinimumSize(Get(), &maximum_size.x, &maximum_size.y);
	return maximum_size;
}

V2_int Window::GetPosition() const {
	V2_int origin;
	SDL_GetWindowPosition(Get(), &origin.x, &origin.y);
	return origin;
}

std::string_view Window::GetTitle() const {
	return SDL_GetWindowTitle(Get());
}

void Window::SetSize(const V2_int& new_size, bool centered) const {
#ifdef __EMSCRIPTEN__
	SetCanvasSize(new_size);
#endif
	SDL_SetWindowSize(Get(), new_size.x, new_size.y);
	// Important to center after resizing.
	if (centered) {
		Center();
	}
}

void Window::SetPosition(const V2_int& new_origin) const {
	SDL_SetWindowPosition(Get(), new_origin.x, new_origin.y);
}

void Window::Center() const {
	SetPosition(V2_int{ SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED });
}

void Window::SetTitle(const std::string& new_title) const {
	return SDL_SetWindowTitle(Get(), new_title.c_str());
}

void Window::SetSetting(WindowSetting setting) const {
	SDL_Window* win{ Get() };
	switch (setting) {
		case WindowSetting::Shown:	  SDL_ShowWindow(win); break;
		case WindowSetting::Hidden:	  SDL_HideWindow(win); break;
		case WindowSetting::Windowed: SDL_SetWindowFullscreen(win, 0); break;
		case WindowSetting::Fullscreen:
			SDL_SetWindowFullscreen(win, SDL_WINDOW_FULLSCREEN_DESKTOP);
			break;
		case WindowSetting::Borderless: SDL_SetWindowBordered(win, SDL_FALSE); break;
		case WindowSetting::Bordered:	SDL_SetWindowBordered(win, SDL_TRUE); break;
		case WindowSetting::Resizable:	SDL_SetWindowResizable(win, SDL_TRUE); break;
		case WindowSetting::FixedSize:	SDL_SetWindowResizable(win, SDL_FALSE); break;
		case WindowSetting::Maximized:	SDL_MaximizeWindow(win); break;
		case WindowSetting::Minimized:	SDL_MinimizeWindow(win); break;
		default:						PTGN_ERROR("Cannot set unrecognized window setting");
	}
}

bool Window::GetSetting(WindowSetting setting) const {
	std::uint32_t flags{ SDL_GetWindowFlags(Get()) };
	switch (setting) {
		case WindowSetting::Shown:	return flags & SDL_WINDOW_SHOWN;
		case WindowSetting::Hidden: return !(flags & SDL_WINDOW_SHOWN);
		case WindowSetting::Windowed:
			return !(flags & (SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_FULLSCREEN));
		case WindowSetting::Fullscreen:
			return (flags & (SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_FULLSCREEN)) ==
				   SDL_WINDOW_FULLSCREEN_DESKTOP;
		case WindowSetting::Borderless: return flags & SDL_WINDOW_BORDERLESS;
		case WindowSetting::Bordered:	return !(flags & SDL_WINDOW_BORDERLESS);
		case WindowSetting::Resizable:	return flags & SDL_WINDOW_RESIZABLE;
		case WindowSetting::FixedSize:	return !(flags & SDL_WINDOW_RESIZABLE);
		case WindowSetting::Maximized:	return flags & SDL_WINDOW_MAXIMIZED;
		case WindowSetting::Minimized:	return flags & SDL_WINDOW_MINIMIZED;
		default:						PTGN_ERROR("Cannot retrieve unrecognized window setting");
	}
}

} // namespace impl

} // namespace ptgn
