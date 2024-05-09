#include "protegon/game.h"
#include "protegon/color.h"

#include "global.h"
#include "game_systems.h"

#include <SDL.h>

#include <chrono>

#if defined(__APPLE__) && defined(USING_APPLE_CLANG)

#include <filesystem>
#include <iostream>
#include <mach-o/dyld.h>

#endif

namespace ptgn {

namespace impl {

void SetupGame() {
// When using AppleClang, for some reason the working directory for the executable is set to $HOME instead of the executable directory.
// Therefore, the C++ code corrects the working directory using std::filesystem so that relative paths work properly.
#if defined(__APPLE__) && defined(USING_APPLE_CLANG)
	char path[1024];
	std::uint32_t size = sizeof(path);
	std::filesystem::path exe_dir;
	if (_NSGetExecutablePath(path, &size) == 0) {
		exe_dir = std::filesystem::path(path).parent_path();
	}
	else {
		std::cout << "Buffer too small to retrieve executable path. Please run the executable from a terminal" << std::endl;
		exe_dir = std::getenv("PWD");
	}
	std::filesystem::current_path(exe_dir);
#endif
	global::impl::game = std::make_unique<Game>();
}

void InitGame() {

}

} // namespace impl

Game::Game() : system_ptr{ new GameSystems() }, systems{ *system_ptr }  {

	
}

Game::~Game() {
	delete system_ptr;
	system_ptr = nullptr;
}

void Game::ConstructWindow(const char* window_title,
					   const V2_int& window_size) {
	auto window{ systems.sdl.GetWindow() };
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

void Game::Loop() {
	using time = std::chrono::time_point<std::chrono::system_clock>;
	time start{ std::chrono::system_clock::now() };
	time end{ std::chrono::system_clock::now() };

	running_ = true;

	while (running_) {
		auto renderer{ systems.sdl.GetRenderer() };
		
		systems.input.Update();

		// Calculate time elapsed during previous frame.
		end = std::chrono::system_clock::now();
		std::chrono::duration<float> elapsed{ end - start };
		float dt{ elapsed.count() };
		start = end;

		Color o{ systems.sdl.GetWindowBackgroundColor() };

		SDL_SetRenderDrawColor(renderer, o.r, o.g, o.b, o.a);
		
		// Clear screen.
		SDL_RenderClear(renderer);

		// Call user update.
		Update(dt);

		// Push drawn objects to screen.
		SDL_RenderPresent(renderer);
	}

	SDL_DestroyWindow(systems.sdl.GetWindow());
}

void Game::Stop() {
	running_ = false;
}

} // namespace ptgn