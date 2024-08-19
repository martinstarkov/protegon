#include "protegon/game.h"

#include "utility/debug.h"
#include "utility/platform.h"

#ifdef __EMSCRIPTEN__

#include <emscripten.h>
#include <emscripten/html5.h>

#endif

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

void Game::PushLoopFunction(const UpdateFunction& loop_function) {
	// Important to clear previous info from input cache (e.g. first time key presses).
	// Otherwise they might trigger again in the next input.Update().
	input.Reset();

	update_stack_.emplace(update_stack_.begin(), loop_function);
}

void Game::PopLoopFunction() {
	input.Reset();

	update_stack_.pop_back();
}

void Game::Stop() {
	update_stack_.clear();
	running_ = false;
}

void Game::Reset() {
	// TODO: Figure out a better way to do this.
	window = {};
	gl_context_.~GLContext();
	new (&gl_context_) impl::GLContext();
	event	  = {};
	input	  = {};
	renderer  = {};
	scene	  = {};
	camera	  = {};
	collision = {};
	music	  = {};
	sound	  = {};
	font	  = {};
	tween	  = {};
	text	  = {};
	texture	  = {};
	shader	  = {};
	profiler  = {};
}

namespace impl {

#ifdef __EMSCRIPTEN__

void EmscriptenLoop(void* data) {
	Game* g = static_cast<Game*>(data);
	g->Update();
	if (!g->running_) {
		emscripten_cancel_main_loop();
	}
}

#endif

} // namespace impl

void Game::MainLoop() {
	// Design decision: Latest possible point to show window is right before
	// loop starts. Comment this if you wish the window to appear hidden for an
	// indefinite period of time.
	window.Show();
#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop_arg(impl::EmscriptenLoop, static_cast<void*>(this), 0, 1);
#else
	while (running_) {
		Update();
	}
#endif
}

void Game::Update() {
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

	if (update_stack_.size() == 0) {
		running_ = false;
		return;
	}

	const auto& loop_function = update_stack_.back();

	game.renderer.Clear();

	if (std::holds_alternative<std::function<void(float)>>(loop_function)) {
		std::get<std::function<void(float)>>(loop_function)(dt);
	} else {
		std::get<std::function<void(void)>>(loop_function)();
	}

	game.renderer.Present();

	++counter;
	end = std::chrono::system_clock::now();
}

} // namespace ptgn