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

Game::~Game() {
	gl_context_.Shutdown();
	sdl_instance_.Shutdown();
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
	Shutdown();
	update_stack_ = {};
	running_	  = false;
}

bool Game::IsRunning() const {
	// Ensure that if game is running, SDL is initialized.
	PTGN_ASSERT(std::invoke([&]() {
		if (running_) {
			return sdl_instance_.IsInitialized();
		}
		return true;
	}));
	return running_;
}

void Game::Init() {
#if defined(PTGN_PLATFORM_MACOS) && !defined(__EMSCRIPTEN__)
	impl::InitApplePath();
#endif

	if (!sdl_instance_.IsInitialized()) {
		sdl_instance_.Init();
	}
	window.Init();
	gl_context_.Init();
	event.Init();
	input.Init();
	renderer.Init();
	collision.Init();
}

void Game::Shutdown() {
	// TODO: Figure out a better way to do this.
	profiler.Reset();
	shader.Reset();
	texture.Reset();
	text.Reset();
	tween.Reset();
	font.Reset();
	sound.Reset();
	music.Reset();

	collision.Shutdown();
	scene.Shutdown();
	renderer.Shutdown();
	input.Shutdown();
	event.Shutdown();
	window.Shutdown();

	// Keep SDL2 instance and OpenGL context alive to ensure SDL2 objects such as TTF_Font are
	// consistent across game instances. For instance: If the user does the following:
	//
	// game.Start<StartScene>();
	// // Inside StartScene:
	// static Font test_font;
	// Text test_text{ test_font };
	// // After window quit start again:
	// game.Start<StartScene>();
	// static Font test_font; // handle already created in the previous SDL initialization.
	// Text test_text{ test_font }; // if SDL has been shutdown, this would not work due to
	// inconsistent SDL versions.
	//
	// Instead, these are called in the Game destructor, which is called upon main() termination.
	// gl_context_.Shutdown();
	// sdl_instance_.Shutdown();
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
	duration<float> elapsed_time{ end - start };

	float elapsed{ elapsed_time.count() };

	float dt{ elapsed };

	// TODO: Consider fixed FPS vs dynamic: https://gafferongames.com/post/fix_your_timestep/.
	// constexpr const float fps{ 60.0f };
	// float frame_time = 1.0f / fps;
	// float dt{ frame_time };

	// if (elapsed < frame_time) {
	//	impl::SDLInstance::Delay(std::chrono::duration_cast<milliseconds>(duration<float>{
	//		frame_time - elapsed }));
	// } // TODO: Add case for when elapsed > dt (such as in Debug mode).
	// PTGN_LOG("Dt: ", dt);

	start = end;

	input.Update();

	tween.Update(dt);

	// PTGN_LOG("Loop #", counter);

	if (update_stack_.size() == 0) {
		running_ = false;
		return;
	}

	const auto& loop_function = update_stack_.back();

	game.renderer.Clear();

	if (std::holds_alternative<std::function<void(float)>>(loop_function)) {
		std::invoke(std::get<std::function<void(float)>>(loop_function), dt);
	} else {
		std::invoke(std::get<std::function<void(void)>>(loop_function));
	}

	if (!running_) {
		return;
	}

	scene.GetTopActive().camera.Update();

	game.renderer.Present();

	++counter;
	end = std::chrono::system_clock::now();

	if (game.profiler.IsEnabled()) {
		game.profiler.PrintAll();
	}
}

} // namespace ptgn