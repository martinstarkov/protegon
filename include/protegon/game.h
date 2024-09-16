#pragma once

#include <functional>
#include <variant>
#include <vector>

#include "core/resource_managers.h"
#include "core/sdl_instance.h"
#include "core/window.h"
#include "event/event_handler.h"
#include "event/input_handler.h"
#include "protegon/collision.h"
#include "protegon/events.h"
#include "renderer/renderer.h"
#include "scene/camera.h"
#include "scene/scene_manager.h"
#include "utility/profiling.h"

namespace ptgn {

namespace impl {

struct WindowDeleter;
struct MixMusicDeleter;
struct MixChunkDeleter;
struct SDL_SurfaceDeleter;
struct TTF_FontDeleter;

static void EmscriptenLoop(void* data);

} // namespace impl

class Game {
public:
	Game() = default;
	~Game();

private:
	Game(const Game&)			 = delete;
	Game(Game&&)				 = delete;
	Game& operator=(const Game&) = delete;
	Game& operator=(Game&&)		 = delete;

private:
	impl::SDLInstance sdl_instance_;

public:
	Window window;

private:
	impl::GLContext gl_context_;

public:
	// Core Subsystems

	EventHandler event;
	InputHandler input;
	Renderer renderer;
	SceneManager scene;
	ActiveSceneCameraManager camera;
	CollisionHandler collision;

	// Resources

	TweenManager tween;
	MusicManager music;
	SoundManager sound;
	FontManager font;
	TextManager text;
	TextureManager texture;
	ShaderManager shader;

	// Debug

	Profiler profiler;

public:
	using UpdateFunction = std::variant<std::function<void()>, std::function<void(float dt)>>;

	void PushLoopFunction(const UpdateFunction& loop_function);
	void PopLoopFunction();

	[[nodiscard]] std::size_t LoopFunctionCount() const {
		return update_stack_.size();
	}

	// Optional: pass in constructor arguments for the start scene.
	template <typename TStartScene, typename... TArgs>
	void Start(TArgs&&... constructor_args) {
		running_ = true;

		Init();

		// Always quit on window quit.
		event.window.Subscribe(
			WindowEvent::Quit, this,
			std::function([this](const WindowQuitEvent&) { PopLoopFunction(); })
		);

		scene.Init<TStartScene>(impl::start_scene_key, std::forward<TArgs>(constructor_args)...);

		PushLoopFunction([&](float dt) { scene.Update(dt); });

		MainLoop();

		Stop();
	}

	void Stop();

	[[nodiscard]] bool IsRunning() const;

private:
	friend struct impl::WindowDeleter;
	friend struct impl::MixMusicDeleter;
	friend struct impl::MixChunkDeleter;
	friend struct impl::SDL_SurfaceDeleter;
	friend struct impl::TTF_FontDeleter;
	friend class impl::GLContext;
	friend void impl::EmscriptenLoop(void* data);

	void MainLoop();
	void Update();
	void Init();
	void Shutdown();

	std::vector<UpdateFunction> update_stack_;

	bool running_{ false };
};

extern Game game;

} // namespace ptgn