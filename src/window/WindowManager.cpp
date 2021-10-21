#include "WindowManager.h"

#include <cassert> // assert

#include <SDL.h>

#include "debugging/Debug.h"
#include "core/SDLManager.h"

namespace ptgn {

namespace impl {

SDLWindowManager::SDLWindowManager() {
	GetSDLManager();
	assert(window_ == nullptr && "Window cannot be created before sdl window manager");
	CreateWindow("Default Title", { 800, 600 }, window::CENTERED, 0);
}

SDLWindowManager::~SDLWindowManager() {
	DestroyWindow();
}

void SDLWindowManager::CreateWindow(const char* title, const V2_int& size, const V2_int& position, std::uint32_t flags) {
	if (window_ == nullptr) {
		window_ = SDL_CreateWindow(title, position.x, position.y, size.x, size.y, flags);
		if (window_ == nullptr) {
			debug::PrintLine("Failed to create window: ", SDL_GetError());
			abort();
		}
	} else {
		SetWindowTitle(title);
		SetWindowSize(size);
	}
}

void SDLWindowManager::DestroyWindow() {
	SDL_DestroyWindow(window_);
	window_ = nullptr;
}

V2_int SDLWindowManager::GetWindowSize() const {
	V2_int size;
	assert(window_ != nullptr && "Cannot get size of non-existent sdl window");
	SDL_GetWindowSize(window_, &size.x, &size.y);
	return size;
}

void SDLWindowManager::SetWindowSize(const V2_int& new_size) {
	assert(window_ != nullptr && "Cannot set size of non-existent sdl window");
	SDL_SetWindowSize(window_, new_size.x, new_size.y);
}

V2_int SDLWindowManager::GetWindowOriginPosition() const {
	V2_int origin;
	assert(window_ != nullptr && "Cannot get origin position of non-existent sdl window");
	SDL_GetWindowPosition(window_, &origin.x, &origin.y);
	return origin;
}

void SDLWindowManager::SetWindowOriginPosition(const V2_int& new_origin) {
	assert(window_ != nullptr && "Cannot set origin position of non-existent sdl window");
	SDL_SetWindowPosition(window_, new_origin.x, new_origin.y);
}

const char* SDLWindowManager::GetWindowTitle() const {
	assert(window_ != nullptr && "Cannot get title of non-existent sdl window");
	return SDL_GetWindowTitle(window_);
}

void SDLWindowManager::SetWindowTitle(const char* new_title) {
	assert(window_ != nullptr && "Cannot set title of non-existent sdl window");
	return SDL_SetWindowTitle(window_, new_title);
}

void SDLWindowManager::SetWindowFullscreen(bool on) {
	assert(window_ != nullptr && "Cannot set non-existent sdl window to fullscreen");
	SDL_SetWindowFullscreen(window_, on);
}

void SDLWindowManager::SetWindowResizeable(bool on) {
	assert(window_ != nullptr && "Cannot set non-existent sdl window to resizeable");
	SDL_SetWindowResizable(window_, static_cast<SDL_bool>(on));
}

SDLWindowManager& GetSDLWindowManager() {
	static SDLWindowManager sdl_window_manager;
	return sdl_window_manager;
}

} // namespace impl

namespace services {

interfaces::WindowManager& GetWindowManager() {
	return impl::GetSDLWindowManager();
}

} // namespace services

} // namespace ptgn