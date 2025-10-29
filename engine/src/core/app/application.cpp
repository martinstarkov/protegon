#include "core/app/application.h"

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "audio/audio.h"
#include "core/app/window.h"
#include "core/input/input_handler.h"
#include "core/util/file.h"
#include "core/util/string.h"
#include "core/util/time.h"
#include "debug/core/log.h"
#include "debug/runtime/assert.h"
#include "debug/runtime/debug_system.h"
#include "debug/runtime/profiling.h"
#include "debug/runtime/stats.h"
#include "math/hash.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "renderer/gl/gl_context.h"
#include "renderer/gl/gl_renderer.h"
#include "renderer/material/shader.h"
#include "renderer/renderer.h"
#include "renderer/text/font.h"
#include "SDL.h"
#include "SDL_error.h"
#include "SDL_hints.h"
#include "SDL_image.h"
#include "SDL_mixer.h"
#include "SDL_timer.h"
#include "SDL_ttf.h"
#include "SDL_version.h"
#include "SDL_video.h"
#include "serialization/json/json.h"
#include "serialization/json/json_manager.h"
#include "world/scene/scene_manager.h"

#ifdef __EMSCRIPTEN__

#include <emscripten.h>
#include <emscripten/html5.h>

EM_JS(int, get_screen_width, (), { return window.screen.width; });
EM_JS(int, get_screen_height, (), { return window.screen.height; });
EM_JS(double, get_device_pixel_ratio, (), { return window.devicePixelRatio || 1.0; });

#endif

#ifdef PTGN_PLATFORM_MACOS

#include <mach-o/dyld.h>

#include <filesystem>
#include <iostream>

#include "CoreFoundation/CoreFoundation.h"

#endif

inline std::ostream& operator<<(std::ostream& os, const SDL_version& v) {
	os << static_cast<int>(v.major) << "." << static_cast<int>(v.minor) << "."
	   << static_cast<int>(v.patch);
	return os;
}

namespace ptgn {

#ifdef __EMSCRIPTEN__

namespace impl {

static EM_BOOL EmscriptenResize(
	int event_type, const EmscriptenUiEvent* ui_event, void* window_ptr
) {
	auto& window{ *static_cast<::ptgn::Window*>(window_ptr) };
	V2_int window_size{ ui_event->windowInnerWidth, ui_event->windowInnerHeight };
	// TODO: Figure out how to deal with itch.io fullscreen button not changing SDL status to
	// fullscreen.
	V2_int screen_size{ get_screen_width(), get_screen_height() };
	if (window_size == screen_size) {
		auto device_pixel_ratio{ get_device_pixel_ratio() };
		window_size = window_size * device_pixel_ratio;
	}
	window.SetSize(window_size);
	return 0;
}

static void EmscriptenInit(Window& window) {
	emscripten_set_resize_callback(
		EMSCRIPTEN_EVENT_TARGET_WINDOW, static_cast<void*>(&window), 0, EmscriptenResize
	);
}

void EmscriptenMainLoop(void* application) {
	auto& app{ *static_cast<Application*>(application) };

	app.Update();

	if (!app.IsRunning()) {
		emscripten_cancel_main_loop();
	}
}

} // namespace impl

#endif

Application::SDLInstance::SDLInstance() {
#if defined(PTGN_PLATFORM_MACOS) && !defined(__EMSCRIPTEN__)
	// When using AppleClang, the working directory for the executable is set to $HOME instead of
	// the executable directory. Therefore, the C++ code corrects the working directory using
	// std::filesystem so that relative paths work properly.
	//
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
	PATH_MAX)) { std::cout << "Couldn't get file system representation! " <<
	std::endl;
	}
	CFRelease(resources_url);
	chdir(path);*/
#endif

#ifdef PTGN_DEBUG
	PTGN_INFO("Build Type: Debug");
#else
	PTGN_INFO("Build Type: Release");
#endif

	std::uint32_t sdl_flags{ SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_TIMER };
	PTGN_ASSERT(
		SDL_WasInit(sdl_flags) != sdl_flags, "Cannot reinitialize SDL instance before shutting down"
	);

	// Ensures window and elements scale by monitor zoom level for constant
	// appearance.
	SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");

	int sdl_init{ SDL_Init(sdl_flags) };
	PTGN_ASSERT(sdl_init == 0, SDL_GetError());

	SDL_version sdl_version;
	SDL_GetVersion(&sdl_version);
	PTGN_INFO("Initialized SDL version: ", sdl_version);

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, PTGN_OPENGL_CONTEXT_PROFILE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, PTGN_OPENGL_MAJOR_VERSION);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, PTGN_OPENGL_MINOR_VERSION);

	int img_flags{ IMG_INIT_PNG | IMG_INIT_JPG };

	PTGN_ASSERT(
		IMG_Init(0) != img_flags, "Cannot reinitialize SDL_image instance before shutting down"
	);

	int img_init{ IMG_Init(img_flags) };

	PTGN_ASSERT(img_init == img_flags, IMG_GetError());

	const SDL_version* sdl_image_version = IMG_Linked_Version();
	PTGN_INFO("Initialized SDL_image version: ", *sdl_image_version);

	PTGN_ASSERT(TTF_WasInit() == 0, "Cannot reinitialize SDL_ttf instance before shutting down");

	int ttf_init{ TTF_Init() };
	PTGN_ASSERT(ttf_init != -1, TTF_GetError());

	const SDL_version* sdl_ttf_version = TTF_Linked_Version();
	PTGN_INFO("Initialized SDL_ttf version: ", *sdl_ttf_version);

#ifdef PTGN_PLATFORM_MACOS
	int mixer_flags{ MIX_INIT_MP3 | MIX_INIT_OGG };
#else
	int mixer_flags{ MIX_INIT_MP3 | MIX_INIT_OGG | MIX_INIT_OPUS |
					 MIX_INIT_WAVPACK /* | MIX_INIT_FLAC | MIX_INIT_MOD | MIX_INIT_MID*/ };
#endif

#ifdef __EMSCRIPTEN__
	mixer_flags = { MIX_INIT_OGG };
#endif

	PTGN_ASSERT(
		Mix_Init(0) != mixer_flags, "Cannot reinitialize SDL_mixer instance before shutting down"
	);

	if (int mixer_init{ Mix_Init(mixer_flags) }; mixer_init != mixer_flags) {
		PTGN_WARN(Mix_GetError());
	}

	int audio_open{ Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) };
	PTGN_ASSERT(audio_open != -1, Mix_GetError());

	const SDL_version* sdl_mixer_version = Mix_Linked_Version();
	PTGN_INFO("Initialized SDL_mixer version: ", *sdl_mixer_version);
}

Application::SDLInstance::~SDLInstance() {
	Mix_CloseAudio();
	PTGN_INFO("Closed SDL_mixer audio");
	Mix_Quit();
	PTGN_INFO("Deinitialized SDL_mixer");
	TTF_Quit();
	PTGN_INFO("Deinitialized SDL_ttf");
	IMG_Quit();
	PTGN_INFO("Deinitialized SDL_image");
	SDL_Quit();
	PTGN_INFO("Deinitialized SDL");
}

Application::Application(const ApplicationConfig& config) :
	window_{ config.title, config.window_size },
	renderer_{ config.window_size },
	scenes_{},
	input_{ window_, renderer_, scenes_ },
	assets_{} {
	// TODO: Move to application config.
	window_.SetSetting(WindowSetting::FixedSize);
}

bool Application::IsRunning() const {
	return running_;
}

void Application::EnterMainLoop() {
	// Design decision: Latest possible point to show window is right before
	// loop starts. Comment this if you wish the window to appear hidden for an
	// indefinite period of time.
	window_.SetSetting(WindowSetting::Shown);
	running_ = true;

#ifdef __EMSCRIPTEN__
	EmscriptenInit(window_);
	emscripten_set_main_loop_arg(
		impl::EmscriptenMainLoop, this, /*fps=*/0, /*simulateInfiniteLoop=*/true
	);
#else
	while (IsRunning()) {
		Update();
	}
#endif
}

void Application::Update() {
	debug_.PreUpdate();

	static auto start{ std::chrono::system_clock::now() };
	static auto end{ std::chrono::system_clock::now() };
	// Calculate time elapsed during previous frame. Unit: seconds.
	secondsf elapsed_time{ end - start };

	float elapsed{ elapsed_time.count() };

	dt_ = elapsed;

	// TODO: Consider fixed FPS vs dynamic: https://gafferongames.com/post/fix_your_timestep/.
	/*constexpr const float fps{ 60.0f };
	dt_ = 1.0f / fps;*/

	/*if (elapsed < dt_) {
		impl::SDLInstance::Delay(to_duration<milliseconds>(secondsf{
			dt_ - elapsed }));
	}*/ // TODO: Add accumulator for when elapsed > dt (such as in Debug mode).
	// PTGN_LOG("Dt: ", dt_);

	start = end;

	scene_.Update(dt_);

	debug_.PostUpdate();

	end = std::chrono::system_clock::now();
}

void Application::Stop() {
	running_ = false;
}

float Application::dt() const {
	return dt_;
}

float Application::time() const {
	// TODO: Consider casting to chrono duration instead.
	return static_cast<float>(SDL_GetTicks64());
}

} // namespace ptgn