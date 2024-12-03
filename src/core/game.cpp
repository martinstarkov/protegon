#include "core/game.h"

#include <chrono>
#include <type_traits>
#include <vector>

#include "audio/audio.h"
#include "camera/camera.h"
#include "collision/collision.h"
#include "core/gl_context.h"
#include "core/manager.h"
#include "core/sdl_instance.h"
#include "core/window.h"
#include "event/event_handler.h"
#include "event/input_handler.h"
#include "physics/physics.h"
#include "renderer/font.h"
#include "renderer/renderer.h"
#include "renderer/shader.h"
#include "renderer/text.h"
#include "renderer/texture.h"
#include "scene/scene_manager.h"
#include "SDL_timer.h"
#include "ui/ui.h"
#include "utility/debug.h"
#include "utility/profiling.h"
#include "utility/time.h"
#include "utility/tween.h"
#include "vfx/light.h"

#ifdef __EMSCRIPTEN__

#include <emscripten.h>
#include <emscripten/html5.h>

EM_JS(int, get_screen_width, (), { return screen.width; });
EM_JS(int, get_screen_height, (), { return screen.height; });

#endif

#ifdef PTGN_PLATFORM_MACOS

#include <mach-o/dyld.h>

#include <filesystem>
#include <iostream>

#include "CoreFoundation/CoreFoundation.h"

#endif
#include <memory>

namespace ptgn {

impl::Game game;

namespace impl {

#ifdef __EMSCRIPTEN__

static EM_BOOL EmscriptenResize(
	int event_type, const EmscriptenUiEvent* ui_event, void* user_data
) {
	V2_int window_size{ ui_event->windowInnerWidth, ui_event->windowInnerHeight };
	// TODO: Figure out how to deal with itch.io fullscreen button not changing SDL status to
	// fullscreen.
	/*V2_int screen_size{ get_screen_width(), get_screen_height() };
	if (window_size == screen_size) {
		// Update fullscreen status? This seems to screw up the camera somehow. Investigate further.
	}*/
	game.window.SetSize(window_size);
	return 0;
}

void EmscriptenInit() {
	emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, 0, 0, EmscriptenResize);
}

void EmscriptenLoop() {
	game.Update();

	if (!game.running_) {
		emscripten_cancel_main_loop();
	}
}

#endif

#ifdef PTGN_PLATFORM_MACOS

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

#endif

Game::Game() :
	sdl_instance_{ std::make_unique<SDLInstance>() },
	window_{ std::make_unique<Window>() },
	gl_context_{ std::make_unique<GLContext>() },
	event_{ std::make_unique<EventHandler>() },
	input_{ std::make_unique<InputHandler>() },
	draw_{ std::make_unique<Renderer>() },
	scene_{ std::make_unique<SceneManager>() },
	camera_{ std::make_unique<SceneCamera>() },
	physics_{ std::make_unique<Physics>() },
	collision_{ std::make_unique<CollisionHandler>() },
	ui_{ std::make_unique<UserInterface>() },
	tween_{ std::make_unique<TweenManager>() },
	music_{ std::make_unique<MusicManager>() },
	sound_{ std::make_unique<SoundManager>() },
	font_{ std::make_unique<FontManager>() },
	text_{ std::make_unique<TextManager>() },
	texture_{ std::make_unique<TextureManager>() },
	shader_{ std::make_unique<ShaderManager>() },
	light_{ std::make_unique<LightManager>() },
	profiler_{ std::make_unique<Profiler>() },
	window{ *window_ },
	event{ *event_ },
	input{ *input_ },
	draw{ *draw_ },
	scene{ *scene_ },
	camera{ *camera_ },
	physics{ *physics_ },
	collision{ *collision_ },
	ui{ *ui_ },
	tween{ *tween_ },
	music{ *music_ },
	sound{ *sound_ },
	font{ *font_ },
	text{ *text_ },
	texture{ *texture_ },
	shader{ *shader_ },
	light{ *light_ },
	profiler{ *profiler_ } {}

Game::~Game() {
	gl_context_->Shutdown();
	sdl_instance_->Shutdown();
}

float Game::dt() const {
	return dt_;
}

float Game::time() const {
	// TODO: Consider casting to chrono duration instead.
	return static_cast<float>(SDL_GetTicks64());
}

void Game::PushFrontLoopFunction(const UpdateFunction& loop_function) {
	// Important to clear previous info from input cache (e.g. first time key presses).
	// Otherwise they might trigger again in the next input.Update().
	input.Reset();

	update_stack_.emplace(update_stack_.begin(), loop_function);
}

void Game::PushBackLoopFunction(const UpdateFunction& loop_function) {
	// Important to clear previous info from input cache (e.g. first time key presses).
	// Otherwise they might trigger again in the next input.Update().
	input.Reset();

	update_stack_.emplace_back(loop_function);
}

void Game::PopBackLoopFunction() {
	if (update_stack_.empty()) {
		return;
	}

	input.Reset();

	update_stack_.pop_back();
}

void Game::PopFrontLoopFunction() {
	if (update_stack_.empty()) {
		return;
	}

	input.Reset();

	update_stack_.erase(update_stack_.begin());
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
			return sdl_instance_->IsInitialized();
		}
		return true;
	}));
	return running_;
}

void Game::Init() {
#if defined(PTGN_PLATFORM_MACOS) && !defined(__EMSCRIPTEN__)
	impl::InitApplePath();
#endif
	running_ = true;
	if (!sdl_instance_->IsInitialized()) {
		sdl_instance_->Init();
	}
	font.Init();
	window.Init();
	gl_context_->Init();
	event.Init();
	input.Init();

	camera.Init();

	shader.Init();
	draw.Init();
	physics.Init();
	light.Init();
}

void Game::Shutdown() {
	scene.Shutdown();

	// TODO: Simply reset all the unique pointers instead of doing this.
	profiler.Reset();
	shader.Reset();
	texture.Reset();
	text.Reset();
	tween.Reset();
	font.Reset();
	sound.Reset();
	music.Reset();

	physics.Shutdown();
	draw.Shutdown();
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

void Game::MainLoop() {
	// Design decision: Latest possible point to show window is right before
	// loop starts. Comment this if you wish the window to appear hidden for an
	// indefinite period of time.
	window.SetSetting(WindowSetting::Shown);
#ifdef __EMSCRIPTEN__
	EmscriptenInit();
	emscripten_set_main_loop(EmscriptenLoop, 0, 1);
#else
	window.SetSetting(WindowSetting::FixedSize);
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

	dt_ = elapsed;

	// TODO: Consider fixed FPS vs dynamic: https://gafferongames.com/post/fix_your_timestep/.
	constexpr const float fps{ 60.0f };
	dt_ = 1.0f / fps;

	if (elapsed < dt_) {
		impl::SDLInstance::Delay(std::chrono::duration_cast<milliseconds>(duration<float>{
			dt_ - elapsed }));
	} // TODO: Add accumulator for when elapsed > dt (such as in Debug mode).
	// PTGN_LOG("Dt: ", dt_);

	start = end;

	input.Update();

	if (!running_) {
		return;
	}

	tween.Update();

	if (!running_) {
		return;
	}

	// PTGN_LOG("Loop #", counter);

	game.draw.Clear();

	if (update_stack_.empty()) {
		scene.Update();
	} else {
		game.scene.SetSceneChanged(false);
		std::invoke(update_stack_.back());
	}

	if (running_ && game.profiler.IsEnabled()) {
		game.profiler.PrintAll();
	}

	if (running_ && !game.scene.SceneChanged()) {
		game.draw.Present();
	}

	++counter;
	end = std::chrono::system_clock::now();
}

} // namespace impl

} // namespace ptgn