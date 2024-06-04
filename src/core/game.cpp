#include "game.h"

#include "protegon/window.h"

#include <SDL.h>

#include <chrono>

#ifdef __APPLE__
#include "CoreFoundation/CoreFoundation.h"
#endif

namespace ptgn {

namespace impl {

void InitializeFileSystem() {
	// TODO: Check if needed:
#ifdef __APPLE__
	/*CFBundleRef main_bundle = CFBundleGetMainBundle();
	CFURLRef resources_url = CFBundleCopyResourcesDirectoryURL(main_bundle);
	char path[PATH_MAX];
	if (!CFURLGetFileSystemRepresentation(resources_url, TRUE, (UInt8*)path, PATH_MAX)) {
		std::cerr << "Couldn't get file system representation! " << std::endl;
	}
	CFRelease(resources_url);
	chdir(path);*/
#endif
}

} // namespace impl

void Game::Loop() {
	// Design decision: Latest possible point to show window is right before loop starts.
	// Comment this if you wish the window to appear hidden for an indefinite period of time.
	window::Show();
	using time = std::chrono::time_point<std::chrono::system_clock>;
	time start{ std::chrono::system_clock::now() };
	time end{ std::chrono::system_clock::now() };

	running_ = true;

	while (running_) {
		auto renderer{ sdl.GetRenderer() };

		input.Update();

		// Calculate time elapsed during previous frame.
		end = std::chrono::system_clock::now();
		std::chrono::duration<float> elapsed{ end - start };
		float dt{ elapsed.count() };
		start = end;

		Color o{ sdl.GetWindowBackgroundColor() };

		SDL_SetRenderDrawColor(renderer.get(), o.r, o.g, o.b, o.a);

		// Clear screen.
		SDL_RenderClear(renderer.get());

		// Call user update on active scenes.
		scene::Update(dt);

		// Push drawn objects to screen.
		SDL_RenderPresent(renderer.get());
	}
}

void Game::Stop() {
	running_ = false;
}

namespace global {

namespace impl {

std::unique_ptr<Game> game;

void InitGame() {
	ptgn::impl::InitializeFileSystem();
	game = std::make_unique<Game>();
}

} // namespace impl

Game& GetGame() {
	assert(impl::game != nullptr && "Game not initialized or destroyed early");
	return *impl::game;
}

} // namespace global

} // namespace ptgn