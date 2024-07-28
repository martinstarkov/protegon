#include "protegon/game.h"

#include "SDL.h"
#include "SDL_image.h"
#include "SDL_mixer.h"
#include "SDL_ttf.h"
#include "renderer/gl_loader.h"
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
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
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

namespace gl {

// Must be called after SDL and window have been initialized.
static void InitOpenGL() {
#define GLE(name, caps_name) \
	name = (PFNGL##caps_name##PROC)SDL_GL_GetProcAddress(PTGN_STRINGIFY_MACRO(gl##name));
	GL_LIST
#undef GLE

	// PTGN_INFO("OpenGL Version: ", glGetString(GL_VERSION));

// For debugging which commands were not initialized.
#define GLE(name, caps_name)                                       \
	if (!(name)) {                                                 \
		PTGN_ERROR("Failed to load ", PTGN_STRINGIFY_MACRO(name)); \
	}
	GL_LIST
#undef GLE

// Check that each of the loaded gl functions was found.
#define GLE(name, caps_name) name&&
	bool gl_init = GL_LIST true;
#undef GLE
	if (!gl_init) {
		PTGN_ERROR("Failed to initialize OpenGL");
		PTGN_CHECK(false, "Failed to initialize OpenGL");
	}
}

} // namespace gl

namespace impl {

GameInstance::GameInstance(Game& g) {
#ifdef PTGN_PLATFORM_MACOS
	InitApplePath();
#endif
	sdl::InitSDL();
	sdl::InitSDLImage();
	sdl::InitSDLTTF();
	sdl::InitSDLMixer();
	g.window.Init();
	gl::InitOpenGL();
	g.renderer.Init();
	g.input.Init();
}

GameInstance::~GameInstance() {
	Mix_CloseAudio();
	Mix_Quit();
	TTF_Quit();
	IMG_Quit();
	SDL_Quit();
}

} // namespace impl

void Game::RepeatUntilQuit(UpdateFunction while_not_quit) {
	bool running = true;

	std::size_t counter = 0;
	using time			= std::chrono::time_point<std::chrono::system_clock>;
	time start{ std::chrono::system_clock::now() };
	time end{ std::chrono::system_clock::now() };

	event.window.Subscribe(
		WindowEvent::Quit, (void*)&running,
		std::function([&](const WindowQuitEvent& e) { running = false; })
	);

	auto update_function = [&]() {
		// Calculate time elapsed during previous frame.
		end = std::chrono::system_clock::now();
		std::chrono::duration<float> elapsed{ end - start };
		float dt{ elapsed.count() };
		start = end;

		input.Update();
		// For debugging:
		// PTGN_LOG("Updating ", counter);

		if (std::holds_alternative<std::function<void(float)>>(while_not_quit)) {
			std::get<std::function<void(float)>>(while_not_quit)(dt);
		} else {
			std::get<std::function<void(void)>>(while_not_quit)();
		}
		++counter;
	};

	// Optional: Update window while it is being dragged. Upside: No rendering artefacts; Downside:
	// window dragging becomes laggier.
	// If enabling this, it is adviseable to change Renderer::Init such that the renderer viewport
	// is updated during window resizing instead of after it has been resized.
	/*event.window.Subscribe(
		WindowEvent::Drag, (void*)this,
		std::function([&](const WindowDragEvent& e) { update_function(); })
	);*/

	while (running && instance_ != nullptr) {
		update_function();
	}

	event.window.Unsubscribe((void*)&running);
}

void Game::Stop() {
	if (instance_ == nullptr) {
		return;
	}
	event.window.Post(WindowEvent::Quit, WindowQuitEvent{});
	*this = {};
	instance_.reset(nullptr);
}

} // namespace ptgn