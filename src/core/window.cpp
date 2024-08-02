#include "core/window.h"

#include "SDL.h"
#include "utility/debug.h"

namespace ptgn {

namespace impl {

void WindowDeleter::operator()(SDL_Window* window) {
	SDL_DestroyWindow(window);
	PTGN_INFO("Destroyed SDL2 window");
}

} // namespace impl

Window::Window() {
	window_.reset(SDL_CreateWindow("", 0, 0, 0, 0, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN));
	PTGN_ASSERT(window_ != nullptr, SDL_GetError());
	PTGN_INFO("Created SDL2 window");
}

void Window::SwapBuffers() {
	SDL_GL_SwapWindow(window_.get());
}

V2_int Window::GetSize() {
	PTGN_ASSERT(Exists(), "Cannot get size of nonexistent window");
	V2_int size;
	SDL_GL_GetDrawableSize(window_.get(), &size.x, &size.y);
	return size;
}

V2_float Window::GetCenter() {
	return GetSize() / 2.0f;
}

V2_int Screen::GetSize() {
	SDL_DisplayMode dm;
	int result = SDL_GetDesktopDisplayMode(0, &dm);
	PTGN_ASSERT(result == 0, "Failed to retrieve screen size: ", SDL_GetError());
	return V2_int{ dm.w, dm.h };
}

void Window::SetupSize(
		const V2_int& resolution, const V2_int& minimum_resolution, bool fullscreen,
		bool borderless, bool resizeable, const V2_float& scale
) {
	SetMinimumSize(minimum_resolution);
	SetFullscreen(fullscreen);

	if (fullscreen) {
		return;
	}

	SetBorderless(borderless);

	if (borderless) {
		SetSize(Screen::GetSize());
	} else {
		SetSize(resolution);
		SetResizeable(resizeable);
	}
}

bool Window::Exists() {
	return window_ != nullptr;
}

void Window::SetMinimumSize(const V2_int& minimum_size) {
	PTGN_ASSERT(Exists(), "Cannot set minimum size of nonexistent window");
	SDL_SetWindowMinimumSize(window_.get(), minimum_size.x, minimum_size.y);
}

V2_int Window::GetMinimumSize() {
	PTGN_ASSERT(Exists(), "Cannot get minimum size of nonexistent window");
	V2_int minimum_size;
	SDL_GetWindowMinimumSize(window_.get(), &minimum_size.x, &minimum_size.y);
	return minimum_size;
}

V2_int Window::GetOriginPosition() {
	PTGN_ASSERT(Exists(), "Cannot get origin position of nonexistent window");
	V2_int origin;
	SDL_GetWindowPosition(window_.get(), &origin.x, &origin.y);
	return origin;
}

const char* Window::GetTitle() {
	PTGN_ASSERT(Exists(), "Cannot get title of nonexistent window");
	return SDL_GetWindowTitle(window_.get());
}

void Window::SetSize(const V2_int& new_size, bool centered) {
	PTGN_ASSERT(Exists(), "Cannot set size of nonexistent window");
	SDL_SetWindowSize(window_.get(), new_size.x, new_size.y);
	// Important to center after resizing.
	if (centered) {
		Center();
	}
}

void Window::SetPosition(const V2_int& new_origin) {
	PTGN_ASSERT(Exists(), "Cannot set origin position of nonexistent window");
	SDL_SetWindowPosition(window_.get(), new_origin.x, new_origin.y);
}

void Window::Center() {
	SetPosition({ SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED });
}

void Window::SetTitle(const char* new_title) {
	PTGN_ASSERT(Exists(), "Cannot set title of nonexistent window");
	return SDL_SetWindowTitle(window_.get(), new_title);
}

void Window::SetFullscreen(bool on) {
	PTGN_ASSERT(Exists(), "Cannot toggle nonexistent window fullscreen");
	if (on) {
		SDL_SetWindowFullscreen(window_.get(), SDL_WINDOW_FULLSCREEN_DESKTOP);
	} else {
		SDL_SetWindowFullscreen(window_.get(), 0); // windowed mode.
	}
}

void Window::SetResizeable(bool on) {
	PTGN_ASSERT(Exists(), "Cannot toggle nonexistent window resizeability");
	SDL_SetWindowResizable(window_.get(), static_cast<SDL_bool>(on));
}

void Window::SetBorderless(bool on) {
	PTGN_ASSERT(Exists(), "Cannot toggle nonexistent window bordered");
	SDL_SetWindowBordered(window_.get(), static_cast<SDL_bool>(!on));
}

void Window::Maximize() {
	PTGN_ASSERT(Exists(), "Cannot maximize nonexistent window");
	SDL_MaximizeWindow(window_.get());
}

void Window::Minimize() {
	PTGN_ASSERT(Exists(), "Cannot minimize nonexistent window");
	SDL_MinimizeWindow(window_.get());
}

void Window::Show() {
	PTGN_ASSERT(Exists(), "Cannot show nonexistent window");
	SDL_ShowWindow(window_.get());
}

void Window::Hide() {
	PTGN_ASSERT(Exists(), "Cannot hide nonexistent window");
	SDL_HideWindow(window_.get());
}

} // namespace ptgn