#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "audio/audio.h"
#include "core/app/game.h"
#include "core/app/sdl_instance.h"
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
#include "renderer/materials/shader.h"
#include "renderer/materials/texture.h"
#include "renderer/renderer.h"
#include "renderer/text/font.h"
#include "SDL_timer.h"
#include "serialization/json/json.h"
#include "serialization/json/json_manager.h"
#include "world/scene/scene_manager.h"

namespace ptgn {

impl::Game game;

namespace impl {

Game::Game() :
	sdl_instance_{ std::make_unique<SDLInstance>() },
	window_{ std::make_unique<Window>() },
	window{ *window_ },
	gl_context_{ std::make_unique<GLContext>() },
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
	debug_{ std::make_unique<DebugSystem>() },
	debug{ *debug_ } {
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
	input.Init();

	shader.Init();
	renderer.Init();
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

void Game::Init(const std::string& title, const V2_int& game_size) {
	window.SetTitle(title);
	// Order matters here.
	window.SetSize(game_size);
	renderer.SetGameSize(game_size);
	window.SetSetting(WindowSetting::FixedSize);
}

void Game::Shutdown() {
	scene.Shutdown();

	sound.Stop(-1);
	music.Stop();

	// TODO: Simply reset all the unique pointers instead of doing this.
	debug.Shutdown();

	renderer.Shutdown();
	shader.Shutdown();
	input.Shutdown();
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
	PTGN_ASSERT(window.IsValid(), "Game must be initialized before entering a scene");
	// Design decision: Latest possible point to show window is right before
	// loop starts. Comment this if you wish the window to appear hidden for an
	// indefinite period of time.
	window.SetSetting(WindowSetting::Shown);
	running_ = true;
#ifdef __EMSCRIPTEN__
	EmscriptenInit();
	emscripten_set_main_loop(EmscriptenLoop, 0, 1);
	// TODO: emscripten_set_main_loop_arg(em_arg_callback_func func, void *arg, int fps, bool
	// simulate_infinite_loop)
	// From: https://emscripten.org/docs/api_reference/emscripten.h.html#c.emscripten_set_main_loop
#else
	while (running_) {
		Update();
	}
	Shutdown();
#endif
}

void Game::Update() {
	debug.PreUpdate();

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

	scene.Update(*this);

	debug.PostUpdate();

	end = std::chrono::system_clock::now();
}

} // namespace impl

} // namespace ptgn