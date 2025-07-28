#include "core/game.h"

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "audio/audio.h"
#include "common/assert.h"
#include "core/sdl_instance.h"
#include "core/time.h"
#include "core/window.h"
#include "debug/debugging.h"
#include "debug/log.h"
#include "debug/profiling.h"
#include "debug/stats.h"
#include "events/event_handler.h"
#include "events/input_handler.h"
#include "math/hash.h"
#include "math/vector2.h"
#include "rendering/api/color.h"
#include "rendering/gl/gl_context.h"
#include "rendering/renderer.h"
#include "rendering/resources/font.h"
#include "rendering/resources/shader.h"
#include "rendering/resources/texture.h"
#include "scene/scene_manager.h"
#include "SDL_timer.h"
#include "serialization/json.h"
#include "serialization/json_manager.h"
#include "utility/file.h"
#include "utility/string.h"

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
		game.Shutdown();
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
	PATH_MAX)) { std::cout << "Couldn't get file system representation! " <<
	std::endl;
	}
	CFRelease(resources_url);
	chdir(path);*/
}

#endif

Game::Game() :
	sdl_instance_{ std::make_unique<SDLInstance>() },
	window_{ std::make_unique<Window>() },
	window{ *window_ },
	gl_context_{ std::make_unique<GLContext>() },
	event_{ std::make_unique<EventHandler>() },
	event{ *event_ },
	input_{ std::make_unique<InputHandler>() },
	input{ *input_ },
	renderer_{ std::make_unique<Renderer>() },
	renderer{ *renderer_ },
	scene_{ std::make_unique<SceneManager>() },
	scene{ *scene_ },
	music_{ std::make_unique<MusicManager>() },
	music{ *music_ },
	sound_{ std::make_unique<SoundManager>() },
	sound{ *sound_ },
	json_{ std::make_unique<JsonManager>() },
	json{ *json_ },
	font_{ std::make_unique<FontManager>() },
	font{ *font_ },
	texture_{ std::make_unique<TextureManager>() },
	texture{ *texture_ },
	shader_{ std::make_unique<ShaderManager>() },
	shader{ *shader_ },
	/*light_{ std::make_unique<LightManager>() },
	light{ *light_ },*/
	profiler_{ std::make_unique<Profiler>() },
	profiler{ *profiler_ } {
	// TODO: Move all of this init code into respective constructors.
#if defined(PTGN_PLATFORM_MACOS) && !defined(__EMSCRIPTEN__)
	impl::InitApplePath();
#endif
	if (!sdl_instance_->IsInitialized()) {
		sdl_instance_->Init();
	}
	font.Init();
	window.Init();
	gl_context_->Init();
	event.Init();
	input.Init();

	shader.Init();
	renderer.Init();
	// light.Init();
}

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

void Game::Stop() {
	running_ = false;
}

bool Game::IsInitialized() const {
	return gl_context_->IsInitialized() && sdl_instance_->IsInitialized();
}

bool Game::IsRunning() const {
	return running_;
}

void Game::Init(
	const std::string& title, const V2_int& window_size, const Color& background_color
) {
	renderer.SetBackgroundColor(background_color);
	game.window.SetTitle(title);
	game.window.SetSize(window_size);
}

void Game::Shutdown() {
	scene.Shutdown();

	sound.Stop(-1);
	music.Stop();
	// TODO: Simply reset all the unique pointers instead of doing this.
	profiler.Reset();

	renderer.Shutdown();
	input.Shutdown();
	event.Shutdown();
	window.Shutdown();

	// Keep SDL2 instance and OpenGL context alive to ensure SDL2 objects such as TTF_Font are
	// consistent across game instances. For instance: If the user does the following:
	//
	// game.scene.Enter<StartScene>();
	// // Inside StartScene:
	// static Font test_font;
	// Text test_text{ test_font };
	// // After window quit start again:
	// game.scene.Enter<StartScene>();
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
	running_ = true;
#ifdef __EMSCRIPTEN__
	EmscriptenInit();
	emscripten_set_main_loop(EmscriptenLoop, 0, 1);
#else
	window.SetSetting(WindowSetting::FixedSize);
	while (running_) {
		Update();
	}
	Shutdown();
#endif
}

void Game::Update() {
	profiler.Clear();

	static auto start{ std::chrono::system_clock::now() };
	static auto end{ std::chrono::system_clock::now() };
	// Calculate time elapsed during previous frame. Unit: seconds.
	duration<float, seconds::period> elapsed_time{ end - start };

	float elapsed{ elapsed_time.count() };

	dt_ = elapsed;

	// TODO: Consider fixed FPS vs dynamic: https://gafferongames.com/post/fix_your_timestep/.
	/*constexpr const float fps{ 60.0f };
	dt_ = 1.0f / fps;*/

	/*if (elapsed < dt_) {
		impl::SDLInstance::Delay(std::chrono::duration_cast<milliseconds>(duration<float>{
			dt_ - elapsed }));
	}*/ // TODO: Add accumulator for when elapsed > dt (such as in Debug mode).
	// PTGN_LOG("Dt: ", dt_);

	start = end;

	scene.HandleSceneEvents();

	if (game.scene.GetActiveSceneCount() != 0) {
		renderer.ClearScreen();

		// scene.ClearSceneTargets();

		scene.Update();

		renderer.PresentScreen();
	}

#ifdef PTGN_DEBUG
	// Uncomment to examine the color of the pixel at the mouse position that is drawn to the
	// screen.
	/*PTGN_LOG(
		"Screen Color at Mouse: ",
		renderer.screen_target_.GetPixel(game.input.GetMousePositionWindow())
	);*/
	// game.stats.PrintCollisionOverlap();
	// game.stats.PrintCollisionIntersect();
	// game.stats.PrintCollisionRaycast();
	// game.stats.PrintRenderer();
	// PTGN_LOG("--------------------------------------");
	game.stats.Reset();
#endif

	profiler.PrintAll();

	end = std::chrono::system_clock::now();
}

} // namespace impl

void LoadResource(std::string_view key, const path& resource_path, bool is_music) {
	PTGN_ASSERT(
		FileExists(resource_path),
		"Cannot load non-existent resource file: ", resource_path.string()
	);

	std::string ext{ ToLower(resource_path.extension().string()) };

	PTGN_ASSERT(!ext.empty(), "Resource file extension is invalid: ", resource_path.string());

	bool is_audio{ ext == ".ogg" || ext == ".mp3" || ext == ".wav" || ext == ".opus" };
	bool is_texture{ ext == ".png" || ext == ".jpg" || ext == ".bmp" || ext == ".gif" };
	bool is_font{ ext == ".ttf" };
	bool is_json{ ext == ".json" };

	PTGN_ASSERT(
		(is_music ? is_audio : true),
		"Music resource path must end in a valid audio format extension"
	);

	if (is_texture) {
		game.texture.Load(key, resource_path);
	} else if (is_audio && !is_music) {
		game.sound.Load(key, resource_path);
	} else if (is_audio && is_music) {
		game.music.Load(key, resource_path);
	} else if (is_font) {
		game.font.Load(key, resource_path);
	} else if (is_json) {
		game.json.Load(key, resource_path);
	} /*
	  // TODO: Add shader loading support.
	  else if (ext == ".vert" || ext == ".frag") {
		game.shader.Load(key, p);
	} */
	else {
		PTGN_ERROR(
			"Attempting to load unsupported file extension from resource file: ",
			resource_path.string()
		);
	}
}

void LoadResources(const std::vector<Resource>& resource_paths) {
	for (const auto& [key, filepath, is_music] : resource_paths) {
		LoadResource(key, filepath, is_music);
	}
}

void LoadResources(const path& resource_file, std::string_view music_resource_suffix) {
	auto resources{ LoadJson(resource_file) };

	// Track unique resource keys.
	std::unordered_set<std::size_t> taken_resource_keys;

	for (const auto& item : resources.items()) {
		const auto& key{ item.key() };
		const auto& resource_path{ item.value() };

		auto key_hash{ Hash(key) };

		PTGN_ASSERT(
			taken_resource_keys.count(key_hash) == 0,
			"Resource key should not be repeated more than once: ", key
		);

		taken_resource_keys.insert(key_hash);

		bool is_music{ EndsWith(key, music_resource_suffix) };

		LoadResource(key, resource_path.get<std::string>(), is_music);
	}
}

} // namespace ptgn