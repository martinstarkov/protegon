#include "game.h"

#include <SDL.h>

#include <chrono>

#include "protegon/platform.h"
#include "protegon/renderer.h"
#include "protegon/window.h"

#ifdef PTGN_PLATFORM_MACOS
#include "CoreFoundation/CoreFoundation.h"
#endif

namespace ptgn {

namespace impl {

void InitializeFileSystem() {
	// TODO: Check if needed:
#ifdef PTGN_PLATFORM_MACOS
	/*CFBundleRef main_bundle = CFBundleGetMainBundle();
	CFURLRef resources_url = CFBundleCopyResourcesDirectoryURL(main_bundle);
	char path[PATH_MAX];
	if (!CFURLGetFileSystemRepresentation(resources_url, TRUE, (UInt8*)path,
	PATH_MAX)) { std::cerr << "Couldn't get file system representation! " <<
	std::endl;
	}
	CFRelease(resources_url);
	chdir(path);*/
#endif
}

} // namespace impl

void Game::Loop() {
	// Design decision: Latest possible point to show window is right before
	// loop starts. Comment this if you wish the window to appear hidden for an
	// indefinite period of time.
	window::Show();
	using time = std::chrono::time_point<std::chrono::system_clock>;
	time start{ std::chrono::system_clock::now() };
	time end{ std::chrono::system_clock::now() };

	window::RepeatUntilQuit([&]() {
		// Calculate time elapsed during previous frame.
		end = std::chrono::system_clock::now();
		std::chrono::duration<float> elapsed{ end - start };
		float dt{ elapsed.count() };
		start = end;

		renderer::SetDrawColor(sdl.GetWindowBackgroundColor());
		renderer::Clear();

		// Call user update on active scenes.
		scene::Update(dt);

		renderer::Present();
	});
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
	PTGN_ASSERT(impl::game != nullptr, "Game not initialized or destroyed early");
	return *impl::game;
}

} // namespace global

} // namespace ptgn