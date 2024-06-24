#include "protegon/start.h"

#include "game.h"
#include "protegon/platform.h"
#include "protegon/window.h"

#ifdef PTGN_PLATFORM_MACOS

#include <mach-o/dyld.h>

#include <filesystem>
#include <iostream>

#endif

namespace ptgn {

namespace impl {

void GameStart() {
// When using AppleClang, for some reason the working directory for the
// executable is set to $HOME instead of the executable directory. Therefore,
// the C++ code corrects the working directory using std::filesystem so that
// relative paths work properly.
#ifdef PTGN_PLATFORM_MACOS
	char path[1024];
	std::uint32_t size = sizeof(path);
	std::filesystem::path exe_dir;
	if (_NSGetExecutablePath(path, &size) == 0) {
		exe_dir = std::filesystem::path(path).parent_path();
	} else {
		std::cout << "Buffer too small to retrieve executable path. Please run "
					 "the executable from a terminal"
				  << std::endl;
		exe_dir = std::getenv("PWD");
	}
	std::filesystem::current_path(exe_dir);
#endif
	global::impl::InitGame();
	renderer::SetBlendMode(BlendMode::Blend);
}

void GameLoop() {
	global::GetGame().Loop();
}

void GameRelease() {
	// Release first ensures destruction of Game instance before the
	// global pointer is invalidated. This is necessary because some
	// of the Game subsystems require a reference to the global pointer
	// during their clean up procedures.
	global::impl::game.release();
	global::impl::game.reset();
}

} // namespace impl

namespace game {

void Stop() {
	// Trigger game stop (running = false).
	global::GetGame().event.window_event.Post(WindowQuitEvent{});
}

} // namespace game

} // namespace ptgn