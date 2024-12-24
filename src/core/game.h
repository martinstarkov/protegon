#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "scene/scene_manager.h"
#include "utility/stats.h"

namespace ptgn {

class LightManager;

namespace impl {

class SDLInstance;
class GLContext;
class Window;
class EventHandler;
class InputHandler;
class Renderer;
class SceneManager;
class SceneCamera;
class Physics;
class CollisionHandler;
class UserInterface;
class TweenManager;
class MusicManager;
class SoundManager;
class FontManager;
class TextManager;
class TextureManager;
class ShaderManager;
class Profiler;

struct WindowDeleter;
struct MixMusicDeleter;
struct MixChunkDeleter;
struct SDL_SurfaceDeleter;
struct TTF_FontDeleter;

#ifdef __EMSCRIPTEN__

static void EmscriptenInit();
static void EmscriptenLoop();

#endif

class Game {
public:
	Game();
	~Game();

private:
	Game(const Game&)			 = delete;
	Game(Game&&)				 = delete;
	Game& operator=(const Game&) = delete;
	Game& operator=(Game&&)		 = delete;

public:
	// @return Previous frame time in milliseconds
	[[nodiscard]] float dt() const;
	// @return Milliseconds since the game was started.
	// Note: Technically this is the time since the SDL2 library was initialized, which is done when
	// starting the game.
	[[nodiscard]] float time() const;

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

	// TODO: Make this flag game for shutdown on next loop (exit) instead of immediately shutting
	// down.
	void Stop();

	[[nodiscard]] bool IsRunning() const;

private:
	friend struct WindowDeleter;
	friend struct MixMusicDeleter;
	friend struct MixChunkDeleter;
	friend struct SDL_SurfaceDeleter;
	friend struct TTF_FontDeleter;
	friend class GLContext;
#ifdef __EMSCRIPTEN__
	friend void EmscriptenLoop();
#endif

	void MainLoop();
	void Update();
	void Init();
	void Shutdown();

	std::unique_ptr<SDLInstance> sdl_instance_;
	std::unique_ptr<Window> window_;
	std::unique_ptr<GLContext> gl_context_;

	std::unique_ptr<EventHandler> event_;
	std::unique_ptr<InputHandler> input_;
	std::unique_ptr<Renderer> draw_;
	std::unique_ptr<SceneManager> scene_;
	std::unique_ptr<SceneCamera> camera_;
	std::unique_ptr<Physics> physics_;
	std::unique_ptr<CollisionHandler> collision_;
	std::unique_ptr<UserInterface> ui_;

	std::unique_ptr<TweenManager> tween_;
	std::unique_ptr<MusicManager> music_;
	std::unique_ptr<SoundManager> sound_;
	std::unique_ptr<FontManager> font_;
	std::unique_ptr<TextManager> text_;
	std::unique_ptr<TextureManager> texture_;
	std::unique_ptr<ShaderManager> shader_;
	std::unique_ptr<LightManager> light_;

	std::unique_ptr<Profiler> profiler_;

	std::vector<UpdateFunction> update_stack_;

	bool running_{ false };
	float dt_{ 0.0f };

public:
	// Core Subsystems

	Window& window;
	EventHandler& event;
	InputHandler& input;
	Renderer& draw;
	SceneManager& scene;
	SceneCamera& camera;
	Physics& physics;
	CollisionHandler& collision;
	UserInterface& ui;

	// Resources

	TweenManager& tween;
	MusicManager& music;
	SoundManager& sound;
	FontManager& font;
	TextManager& text;
	TextureManager& texture;
	ShaderManager& shader;
	LightManager& light;

	// Debug

	Profiler& profiler;
#ifdef PTGN_DEBUG
	Stats stats;
#endif
};

} // namespace impl

extern impl::Game game;

} // namespace ptgn
