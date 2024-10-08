#include "core/window.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "core/sdl_instance.h"
#include "protegon/game.h"
#include "protegon/log.h"
#include "protegon/vector2.h"
#include "SDL_error.h"
#include "SDL_mouse.h"
#include "SDL_stdinc.h"
#include "SDL_video.h"
#include "utility/debug.h"

namespace ptgn {

namespace impl {

void WindowDeleter::operator()(SDL_Window* window) const {
	if (game.sdl_instance_.SDLIsInitialized()) {
		SDL_DestroyWindow(window);
		PTGN_INFO("Destroyed SDL2 window");
	}
}

} // namespace impl

void Window::Init() {
	PTGN_ASSERT(!Exists(), "Previous window must be destroyed before initializing a new one");
	window_.reset(SDL_CreateWindow("", 0, 0, 0, 0, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN));
	PTGN_ASSERT(Exists(), SDL_GetError());
	PTGN_INFO("Created SDL2 window");
}

void Window::Shutdown() {
	window_.reset(nullptr);
}

void Window::SwapBuffers() const {
	SDL_GL_SwapWindow(window_.get());
}

V2_int Window::GetSize() const {
	PTGN_ASSERT(Exists(), "Cannot get size of nonexistent window");
	V2_int size;
	SDL_GL_GetDrawableSize(window_.get(), &size.x, &size.y);
	return size;
}

V2_float Window::GetCenter() const {
	return GetSize() / 2.0f;
}

V2_int Screen::GetSize() {
	SDL_DisplayMode dm;
	int result = SDL_GetDesktopDisplayMode(0, &dm);
	PTGN_ASSERT(result == 0, "Failed to retrieve screen size: ", SDL_GetError());
	return V2_int{ dm.w, dm.h };
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

void Window::SetFullscreen(FullscreenMode mode) const {
	PTGN_ASSERT(Exists(), "Cannot toggle nonexistent window fullscreen");
	SDL_SetWindowFullscreen(window_.get(), static_cast<std::uint32_t>(mode));
}

void Window::SetResizeable(bool on) const {
	PTGN_ASSERT(Exists(), "Cannot toggle nonexistent window resizeability");
	SDL_SetWindowResizable(window_.get(), static_cast<SDL_bool>(on));
}

void Window::SetBorderless(bool on) const {
	PTGN_ASSERT(Exists(), "Cannot toggle nonexistent window bordered");
	SDL_SetWindowBordered(window_.get(), static_cast<SDL_bool>(!on));
}

void Window::Maximize() const {
	PTGN_ASSERT(Exists(), "Cannot maximize nonexistent window");
	SDL_MaximizeWindow(window_.get());
}

void Window::Minimize() const {
	PTGN_ASSERT(Exists(), "Cannot minimize nonexistent window");
	SDL_MinimizeWindow(window_.get());
}

void Window::Show() const {
	PTGN_ASSERT(Exists(), "Cannot show nonexistent window");
	SDL_ShowWindow(window_.get());
}

void Window::Hide() const {
	PTGN_ASSERT(Exists(), "Cannot hide nonexistent window");
	SDL_HideWindow(window_.get());
}

} // namespace ptgn