#pragma once

#include <functional>
#include <variant>
#include <vector>

#include "core/sdl_instance.h"
#include "core/window.h"
#include "event/event_handler.h"
#include "event/input_handler.h"
#include "protegon/audio.h"
#include "protegon/collision.h"
#include "protegon/events.h"
#include "protegon/font.h"
#include "protegon/shader.h"
#include "protegon/text.h"
#include "protegon/texture.h"
#include "protegon/tween.h"
#include "renderer/renderer.h"
#include "scene/camera.h"
#include "scene/scene_manager.h"
#include "ui/ui.h"
#include "utility/profiling.h"

namespace ptgn {

namespace impl {

struct WindowDeleter;
struct MixMusicDeleter;
struct MixChunkDeleter;
struct SDL_SurfaceDeleter;
struct TTF_FontDeleter;

static void EmscriptenLoop(void* data);

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
	SDLInstance sdl_instance_;

public:
	Window window;

private:
	GLContext gl_context_;

public:
	// @return Previous frame time in milliseconds
	[[nodiscard]] float dt() const;

	// Core Subsystems

	// TODO: Make these all inside impl namespace instead of hiding constructors.

	EventHandler event;
	InputHandler input;
	Renderer draw;
	SceneManager scene;
	ActiveSceneCameraManager camera;
	CollisionHandler collision;
	UserInterface ui;

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
	using UpdateFunction = std::function<void()>;

	void PushBackLoopFunction(const UpdateFunction& loop_function);
	void PushFrontLoopFunction(const UpdateFunction& loop_function);
	void PopBackLoopFunction();
	void PopFrontLoopFunction();

	[[nodiscard]] std::size_t LoopFunctionCount() const {
		return update_stack_.size();
	}

	// Optional: pass in constructor arguments for the start scene.
	template <typename TStartScene, typename... TArgs>
	void Start(TArgs&&... constructor_args) {
		Init();

		scene.Init<TStartScene>(start_scene_key, std::forward<TArgs>(constructor_args)...);

		MainLoop();

		Stop();
	}

	void Stop();

	[[nodiscard]] bool IsRunning() const;

private:
	friend struct WindowDeleter;
	friend struct MixMusicDeleter;
	friend struct MixChunkDeleter;
	friend struct SDL_SurfaceDeleter;
	friend struct TTF_FontDeleter;
	friend class GLContext;
	friend void EmscriptenLoop(void* data);

	void MainLoop();
	void Update();
	void Init();
	void Shutdown();

	std::vector<UpdateFunction> update_stack_;

	bool running_{ false };
	float dt_{ 0.0f };
};

} // namespace impl

extern impl::Game game;

} // namespace ptgn