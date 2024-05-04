#include "game.h"

#include <cassert>

#if defined(__APPLE__) && defined(USING_APPLE_CLANG)

#include <filesystem>
#include <iostream>
#include <mach-o/dyld.h>

#endif

namespace ptgn {

namespace global {

namespace impl {

std::unique_ptr<Game> game{ nullptr };

} // namespace impl

void InitGame() {
// When using AppleClang, for some reason the working directory for the executable is set to $HOME instead of the executable directory.
// Therefore, the C++ code corrects the working directory using std::filesystem so that relative paths work properly.
#if defined(__APPLE__) && defined(USING_APPLE_CLANG)
    char path[1024];
    std::uint32_t size = sizeof(path);
    std::filesystem::path exe_dir;
    if (_NSGetExecutablePath(path, &size) == 0) {
        exe_dir = std::filesystem::path(path).parent_path();
    } else {
        std::cout << "Buffer too small to retrieve executable path. Please run the executable from a terminal" << std::endl;
        exe_dir = std::getenv("PWD");
    }
    std::filesystem::current_path(exe_dir);
#endif
	impl::game = std::make_unique<Game>();
}

void DestroyGame() {
	impl::game.reset();
}

Game& GetGame() {
	assert(impl::game != nullptr && "Game not initialized?");
	return *impl::game;
}

} // namespace global

} // namespace ptgn
