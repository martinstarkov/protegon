#pragma once

#include <memory>
#include <variant>
#include <vector>

#include "core/gl_context.h"
#include "core/resource_managers.h"
#include "core/sdl_instance.h"
#include "core/window.h"
#include "event/event_handler.h"
#include "event/input_handler.h"
#include "renderer/renderer.h"
#include "scene/scene_manager.h"
#include "utility/profiling.h"

namespace ptgn {

namespace impl {

static void EmscriptenLoop(void* data);

} // namespace impl

class Game {
public:
	Game();
	~Game() = default;

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

	// Optional: pass in constructor arguments for the TStartScene.
	template <typename TStartScene, typename... TArgs>
	void Start(TArgs&&... constructor_args) {
		running_ = true;

		// Always quit on window quit.
		event.window.Subscribe(
			WindowEvent::Quit, (void*)this,
			std::function([&](const WindowQuitEvent& e) { PopLoopFunction(); })
		);

		scene.StartScene<TStartScene>(
			impl::start_scene_key, std::forward<TArgs>(constructor_args)...
		);

		PushLoopFunction([&](float dt) { scene.Update(dt); });

		MainLoop();

		event.window.Unsubscribe((void*)this);
		Reset();
	}

	void Stop();

private:
	friend void impl::EmscriptenLoop(void* data);

	void MainLoop();
	void Update();
	void Reset();

	std::vector<UpdateFunction> update_stack_;

	bool running_{ false };
};

extern Game game;

} // namespace ptgn