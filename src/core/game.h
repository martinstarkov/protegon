#pragma once

#include <memory>
#include <string>

#include "math/vector2.h"
#include "renderer/color.h"
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

	// @return Milliseconds since Init() was called.
	[[nodiscard]] float time() const;

	// Entry point for the game / application.
	// Note: Window will not appear until an active scene has been loaded into the scene manager:
	// game.scene.LoadActive<MyScene>("scene_name");
	// @param title The title of the window. Can be changed later using
	// game.window.SetTitle("title");
	// @param resolution Starting resolution of the window. Can be changed later using
	// game.window.SetSize({ 1920, 1080 });
	// @param background_color Starting background color of the window. Can be changed later using
	// game.renderer.SetClearColor(color::Black);
	void Init(
		const std::string& title = "Default Title", const V2_int& resolution = { 800, 800 },
		const Color& background_color = color::White
	);

	// Stops the game from running.
	void Stop();

	// @return True if the game is running, false if it has been stopped.
	[[nodiscard]] bool IsRunning() const;

private:
	friend struct WindowDeleter;
	friend struct MixMusicDeleter;
	friend struct MixChunkDeleter;
	friend struct SDL_SurfaceDeleter;
	friend struct TTF_FontDeleter;
	friend class GLContext;
	friend class SceneManager;
#ifdef __EMSCRIPTEN__
	friend void EmscriptenLoop();
#endif

	void MainLoop();
	void Update();
	void Shutdown();

	std::unique_ptr<SDLInstance> sdl_instance_;
	std::unique_ptr<Window> window_;
	std::unique_ptr<GLContext> gl_context_;

	std::unique_ptr<EventHandler> event_;
	std::unique_ptr<InputHandler> input_;
	std::unique_ptr<Renderer> renderer_;
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

	bool running_{ false };
	float dt_{ 0.0f };

public:
	// Note: It is important that these are defined below the private unique ptrs so their
	// constructor are called later.

	// Core Subsystems

	Window& window;
	EventHandler& event;
	InputHandler& input;
	Renderer& renderer;
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
