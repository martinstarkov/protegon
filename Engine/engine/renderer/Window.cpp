#include "Window.h"

#include <SDL.h>

namespace engine {

Window::Window(SDL_Window* window) : window{ window } {}
SDL_Window* Window::operator=(SDL_Window* window) { this->window = window; return this->window; }
Window::operator SDL_Window* () const { return window; }
Window::operator bool() const { return window != nullptr; }
SDL_Window* Window::operator&() const { return window; }
void Window::Destroy() { SDL_DestroyWindow(window); window = nullptr; }

} // namespace engine