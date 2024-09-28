#include "core/window.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "SDL_error.h"
#include "SDL_mouse.h"
#include "SDL_stdinc.h"
#include "SDL_video.h"
#include "core/sdl_instance.h"
#include "protegon/game.h"
#include "protegon/log.h"
#include "protegon/vector2.h"
#include "utility/debug.h"

namespace ptgn {

V2_int Screen::GetSize() {
	SDL_DisplayMode dm;
	int result = SDL_GetDesktopDisplayMode(0, &dm);
	PTGN_ASSERT(result == 0, "Failed to retrieve screen size: ", SDL_GetError());
	return V2_int{ dm.w, dm.h };
}

namespace impl {

void WindowDeleter::operator()(SDL_Window* window) const {
	if (game.sdl_instance_.SDLIsInitialized()) {
		SDL_DestroyWindow(window);
		PTGN_INFO("Destroyed SDL2 window");
	}
}

void Window::Init() {
	PTGN_ASSERT(!Exists(), "Previous window must be destroyed before initializing a new one");
	window_.reset(SDL_CreateWindow(
		"", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 0, 0,
		SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN | SDL_WINDOW_ALLOW_HIGHDPI
	));
	PTGN_ASSERT(Exists(), SDL_GetError());
	PTGN_INFO("Created SDL2 window");
}

void Window::Shutdown() {
	window_.reset(nullptr);
}

void Window::SwapBuffers() const {
	SDL_GL_SwapWindow(window_.get());
}

V2_int Window::GetSize(int type) const {
	PTGN_ASSERT(Exists(), "Cannot get size of nonexistent window");
	V2_int size;
	PTGN_ASSERT(
		type == 0 || type == 1 || type == 2, "Unrecognized type provided to window.GetSize"
	);
	if (type == 0) {
		SDL_GL_GetDrawableSize(window_.get(), &size.x, &size.y);
	} else if (type == 1) {
		SDL_GetWindowSizeInPixels(window_.get(), &size.x, &size.y);
	} else if (type == 2) {
		SDL_GetWindowSize(window_.get(), &size.x, &size.y);
	}
	return size;
}

V2_float Window::GetCenter() const {
	return GetSize() / 2.0f;
}

void Window::SetRelativeMouseMode(bool on) const {
	SDL_SetRelativeMouseMode(static_cast<SDL_bool>(on));
}

void Window::SetMouseGrab(bool on) const {
	PTGN_ASSERT(Exists(), "Cannot set grab of nonexistent window");
	SDL_SetWindowMouseGrab(window_.get(), static_cast<SDL_bool>(on));
}

void Window::CaptureMouse(bool on) const {
	SDL_CaptureMouse(static_cast<SDL_bool>(on));
}

void Window::SetAlwaysOnTop(bool on) const {
	PTGN_ASSERT(Exists(), "Cannot set always on top for nonexistent window");
	SDL_SetWindowAlwaysOnTop(window_.get(), static_cast<SDL_bool>(on));
}

bool Window::Exists() const {
	return window_ != nullptr;
}

void Window::SetMinimumSize(const V2_int& minimum_size) const {
	PTGN_ASSERT(Exists(), "Cannot set minimum size of nonexistent window");
	SDL_SetWindowMinimumSize(window_.get(), minimum_size.x, minimum_size.y);
}

V2_int Window::GetMinimumSize() const {
	PTGN_ASSERT(Exists(), "Cannot get minimum size of nonexistent window");
	V2_int minimum_size;
	SDL_GetWindowMinimumSize(window_.get(), &minimum_size.x, &minimum_size.y);
	return minimum_size;
}

void Window::SetMaximumSize(const V2_int& maximum_size) const {
	PTGN_ASSERT(Exists(), "Cannot set maximum size of nonexistent window");
	SDL_SetWindowMaximumSize(window_.get(), maximum_size.x, maximum_size.y);
}

V2_int Window::GetMaximumSize() const {
	PTGN_ASSERT(Exists(), "Cannot get maximum size of nonexistent window");
	V2_int maximum_size;
	SDL_GetWindowMinimumSize(window_.get(), &maximum_size.x, &maximum_size.y);
	return maximum_size;
}

V2_int Window::GetPosition() const {
	PTGN_ASSERT(Exists(), "Cannot get position of nonexistent window");
	V2_int origin;
	SDL_GetWindowPosition(window_.get(), &origin.x, &origin.y);
	return origin;
}

std::string_view Window::GetTitle() const {
	PTGN_ASSERT(Exists(), "Cannot get title of nonexistent window");
	return SDL_GetWindowTitle(window_.get());
}

void Window::SetSize(const V2_int& new_size, bool centered) const {
	PTGN_ASSERT(Exists(), "Cannot set size of nonexistent window");
	SDL_SetWindowSize(window_.get(), new_size.x, new_size.y);
	// Important to center after resizing.
	if (centered) {
		Center();
	}
}

void Window::SetPosition(const V2_int& new_origin) const {
	PTGN_ASSERT(Exists(), "Cannot set origin position of nonexistent window");
	SDL_SetWindowPosition(window_.get(), new_origin.x, new_origin.y);
}

void Window::Center() const {
	SetPosition({ SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED });
}

void Window::SetTitle(const std::string& new_title) const {
	PTGN_ASSERT(Exists(), "Cannot set title of nonexistent window");
	return SDL_SetWindowTitle(window_.get(), new_title.c_str());
}

void Window::SetWindowed() const {
	int flags = SDL_GetWindowFlags(window_.get());
	if ((flags & SDL_WINDOW_FULLSCREEN) == 0) {
		return;
	}
	SDL_SetWindowFullscreen(window_.get(), 0);
}

void Window::SetFullscreen() const {
	int flags = SDL_GetWindowFlags(window_.get());
	if ((flags & SDL_WINDOW_FULLSCREEN) == SDL_WINDOW_FULLSCREEN) {
		return;
	}
	SDL_DisplayMode mode;
	SDL_GetCurrentDisplayMode(0, &mode);
	SDL_SetWindowDisplayMode(window_.get(), &mode);
	SDL_SetWindowFullscreen(window_.get(), SDL_WINDOW_FULLSCREEN);
}

void Window::SetSetting(WindowSetting setting) const {
	PTGN_ASSERT(Exists(), "Cannot set setting of nonexistent window");
	switch (setting) {
		case WindowSetting::Shown:		SDL_ShowWindow(window_.get()); break;
		case WindowSetting::Hidden:		SDL_HideWindow(window_.get()); break;
		case WindowSetting::Windowed:	SetWindowed(); break;
		case WindowSetting::Fullscreen: SetFullscreen(); break;
		case WindowSetting::Borderless: SDL_SetWindowBordered(window_.get(), SDL_FALSE); break;
		case WindowSetting::Bordered:	SDL_SetWindowBordered(window_.get(), SDL_TRUE); break;
		case WindowSetting::Resizable:	SDL_SetWindowResizable(window_.get(), SDL_TRUE); break;
		case WindowSetting::FixedSize:	SDL_SetWindowResizable(window_.get(), SDL_FALSE); break;
		case WindowSetting::Maximized:	SDL_MaximizeWindow(window_.get()); break;
		case WindowSetting::Minimized:	SDL_MinimizeWindow(window_.get()); break;
		default:						PTGN_ERROR("Cannot set unrecognized window setting");
	}
}

bool Window::GetSetting(WindowSetting setting) const {
	PTGN_ASSERT(Exists(), "Cannot get setting of nonexistent window");
	int flags = SDL_GetWindowFlags(window_.get());
	switch (setting) {
		case WindowSetting::Shown:	  return flags & SDL_WINDOW_SHOWN;
		case WindowSetting::Hidden:	  return !(flags & SDL_WINDOW_SHOWN);
		case WindowSetting::Windowed: return (flags & SDL_WINDOW_FULLSCREEN) == 0;
		case WindowSetting::Fullscreen:
			return (flags & SDL_WINDOW_FULLSCREEN) == SDL_WINDOW_FULLSCREEN;
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