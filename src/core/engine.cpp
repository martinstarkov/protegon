#include "protegon/engine.h"
#include "protegon/color.h"

#include "core/sdl_instance.h"

#include <chrono>

namespace ptgn {

Engine::Engine() {
	global::InitSDL();
}

Engine::~Engine() {
	global::DestroySDL();
}

void Engine::Construct(const char* window_title,
					   const V2_int& window_size) {
	auto window{ global::GetSDL().GetWindow() };
	SDL_SetWindowTitle(window,
					   window_title);
	SDL_SetWindowSize(window,
					  window_size.x,
					  window_size.y);
	SDL_SetWindowPosition(window,
						  SDL_WINDOWPOS_CENTERED,
						  SDL_WINDOWPOS_CENTERED);
	SDL_ShowWindow(window);
	Create();
	Loop();
}

void Engine::Loop() {
	using time = std::chrono::time_point<std::chrono::system_clock>;
	time start{ std::chrono::system_clock::now() };
	time end{ std::chrono::system_clock::now() };

	auto sdl{ global::GetSDL() };
	auto renderer{ sdl.GetRenderer() };

	Color window_color{ 255, 255, 255, 255 };

	while (sdl.GetWindow() != nullptr) {
		// TODO: Fetch updated user inputs here.

		// Calculate time elapsed during previous frame.
		end = std::chrono::system_clock::now();
		std::chrono::duration<float> elapsed{ end - start };
		float dt{ elapsed.count() };
		start = end;

		SDL_SetRenderDrawColor(renderer,
							   window_color.r,
							   window_color.g,
							   window_color.b,
							   window_color.a);
		// Clear screen.
		SDL_RenderClear(renderer);

		// Call user update.
		Update(dt);

		// Push drawn objects to screen.
		SDL_RenderPresent(renderer);
	}
}

} // namespace ptgn