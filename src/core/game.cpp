#include "protegon/game.h"

#include "SDL.h"
#include "SDL_image.h"
#include "SDL_mixer.h"
#include "SDL_ttf.h"
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

namespace impl {

#ifdef PTGN_PLATFORM_MACOS
// When using AppleClang, the working directory for the executable is set to $HOME instead of the
// executable directory. Therefore, the C++ code corrects the working directory using
// std::filesystem so that relative paths work properly.
static void InitApplePath() {
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

namespace sdl {

static void InitSDL() {
	std::uint32_t sdl_flags{ SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_TIMER | SDL_INIT_VIDEO |
							 SDL_VIDEO_OPENGL };
	if (!SDL_WasInit(sdl_flags)) {
		SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_SCALING, "1");
		// Ensures window and elements scale by monitor zoom level for constant
		// appearance.
		SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
		SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
		SDL_SetHint(SDL_HINT_RENDER_BATCHING, "1");

		int sdl_init{ SDL_Init(sdl_flags) };
		if (sdl_init != 0) {
			PTGN_ERROR(SDL_GetError());
			PTGN_CHECK(false, "Failed to initialize SDL core");
		}
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, PTGN_OPENGL_MAJOR_VERSION);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, PTGN_OPENGL_MINOR_VERSION);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

		int gl_load = SDL_GL_LoadLibrary(NULL);
		if (gl_load != 0) {
			PTGN_ERROR(SDL_GetError());
			PTGN_CHECK(false, "Failed to load OpenGL library");
		}
	}
}

static void InitSDLImage() {
	int img_flags{ IMG_INIT_PNG | IMG_INIT_JPG };
	int img_init{ IMG_Init(img_flags) };
	if ((img_init & img_flags) != img_flags) {
		PTGN_ERROR(IMG_GetError());
		PTGN_CHECK(false, "Failed to initialize SDL Image");
	}
}

static void InitSDLTTF() {
	int ttf_init{ TTF_Init() };
	if (ttf_init == -1) {
		PTGN_ERROR(TTF_GetError());
		PTGN_CHECK(false, "Failed to initialize SDL TTF");
	}
}

static void InitSDLMixer() {
	int mixer_init{ Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) };
	if (mixer_init == -1) {
		PTGN_ERROR(Mix_GetError());
		PTGN_CHECK(false, "Failed to initialize SDL Mixer");
	}
}

} // namespace sdl

} // namespace impl

namespace impl {

GameInstance::GameInstance(Game& g) {
#ifdef PTGN_PLATFORM_MACOS
	InitApplePath();
#endif
	sdl::InitSDL();
	sdl::InitSDLImage();
	sdl::InitSDLTTF();
	sdl::InitSDLMixer();
}

GameInstance::~GameInstance() {
	Mix_CloseAudio();
	Mix_Quit();
	TTF_Quit();
	IMG_Quit();
	SDL_Quit();
}

} // namespace impl

Game::Game() : instance_{ std::make_unique<impl::GameInstance>(*this) } {}

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
	if (instance_ == nullptr) {
		return;
	}
	event.window.Post(WindowEvent::Quit, WindowQuitEvent{});
	*this = {};
	instance_.reset(nullptr);
}

void Game::SceneLoop() {
	// In case Stop() was called in Scene constructor (non-looping scene).
	if (instance_ == nullptr) {
		return;
	}
	// Design decision: Latest possible point to show window is right before
	// loop starts. Comment this if you wish the window to appear hidden for an
	// indefinite period of time.
	window.Show();
	LoopUntilQuit([&](float dt) {
		renderer.Clear();
		scene.Update(dt);
		if (instance_ == nullptr) {
			return;
		}
		renderer.Present();
	});
}

void Game::Update(const UpdateFunction& loop_function) {
	static std::size_t counter = 0;
	static auto start{ std::chrono::system_clock::now() };
	static auto end{ std::chrono::system_clock::now() };
	// Calculate time elapsed during previous frame.
	end = std::chrono::system_clock::now();
	duration<float> elapsed{ end - start };
	float dt{ elapsed.count() };
	start = end;

	input.Update();
	// For debugging:
	// PTGN_LOG("Updating ", counter);

	if (std::holds_alternative<std::function<void(float)>>(loop_function)) {
		std::get<std::function<void(float)>>(loop_function)(dt);
	} else {
		std::get<std::function<void(void)>>(loop_function)();
	}
	++counter;
}

} // namespace ptgn