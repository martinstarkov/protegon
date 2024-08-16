#include "protegon/game.h"

#include "utility/debug.h"
#include "utility/platform.h"

#ifdef PTGN_PLATFORM_MACOS

#include <mach-o/dyld.h>

#include <filesystem>
#include <iostream>

#include "CoreFoundation/CoreFoundation.h"

#endif

namespace ptgn {

Game game;

#ifdef PTGN_PLATFORM_MACOS

namespace impl {

// When using AppleClang, the working directory for the executable is set to $HOME instead of the
// executable directory. Therefore, the C++ code corrects the working directory using
// std::filesystem so that relative paths work properly.
static void InitApplePath() {
	// TODO: Add check that this hasnt happened yet.
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
	// TODO: Check if needed:
	/*CFBundleRef main_bundle = CFBundleGetMainBundle();
	CFURLRef resources_url = CFBundleCopyResourcesDirectoryURL(main_bundle);
	char path[PATH_MAX];
	if (!CFURLGetFileSystemRepresentation(resources_url, TRUE, (UInt8*)path,
	PATH_MAX)) { std::cerr << "Couldn't get file system representation! " <<
	std::endl;
	}
	CFRelease(resources_url);
	chdir(path);*/
}

} // namespace impl

#endif

Game::Game() {
	// dummy_camera_{ std::make_unique<CameraManager>() } {
	//  camera{ *dummy_camera_ } {
#ifdef PTGN_PLATFORM_MACOS
	impl::InitApplePath();
#endif
}

void Game::LoopUntilKeyDown(
	const std::vector<Key>& any_of_keys, const UpdateFunction& loop_function
) {
	LoopUntilEvent(
		game.event.key, { KeyEvent::Down }, std::function([&](const KeyDownEvent& e) -> bool {
			for (const Key& key : any_of_keys) {
				if (e.key == key) {
					return true;
				}
			}
			return false;
		}),
		loop_function
	);
}

void Game::LoopUntilQuit(const UpdateFunction& loop_function) {
	LoopUntilEvent(
		game.event.window, { WindowEvent::Quit },
		std::function([&](const WindowQuitEvent& e) -> bool { return true; }), loop_function
	);
}

void Game::Stop() {
	running_ = false;
}

void Game::Reset() {
	window = {};
	gl_context_.~GLContext();
	new (&gl_context_) impl::GLContext();
	event	 = {};
	input	 = {};
	renderer = {};
	scene	 = {};
	camera	 = {};
	music	 = {};
	sound	 = {};
	font	 = {};
	text	 = {};
	texture	 = {};
	shader	 = {};
	profiler = {};
}

void Game::MainLoop() {
	static UpdateFunction main = std::function([&](float dt) {
		renderer.Clear();
		scene.Update(dt);
		renderer.Present();
	});
	LoopFunction(main);
}

void Game::LoopFunction(const UpdateFunction& loop_function) {
	static std::size_t counter{ 0 };
	static auto start{ std::chrono::system_clock::now() };
	static auto end{ std::chrono::system_clock::now() };
	// Calculate time elapsed during previous frame.
	duration<float> elapsed{ end - start };
	float dt{ elapsed.count() };
	start = end;

	input.Update();

	tween.Update(dt);

	scene.GetTopActive().camera.Update();
	// PTGN_LOG("Loop #", counter);

	if (std::holds_alternative<std::function<void(float)>>(loop_function)) {
		std::get<std::function<void(float)>>(loop_function)(dt);
	} else {
		std::get<std::function<void(void)>>(loop_function)();
	}
	++counter;
	end = std::chrono::system_clock::now();
}

} // namespace ptgn