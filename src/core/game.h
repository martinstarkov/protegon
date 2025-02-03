#pragma once

#include <memory>
#include <string>

#include "ecs/ecs.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "scene/scene_manager.h"
#include "utility/stats.h"

namespace ptgn {

class LightManager;
class RenderTarget;

namespace impl {

class SDLInstance;
class GLContext;
class Window;
class EventHandler;
class InputHandler;
class Renderer;
class SceneManager;
class CameraManager;
class JsonManager;
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
struct Mix_MusicDeleter;
struct Mix_ChunkDeleter;
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
	// game.scene.Enter<MyScene>("scene_name");
	// @param title The title of the window. Can be changed later using
	// game.window.SetTitle("title");
	// @param window_size Starting size of the window. Can be changed later using
	// game.window.SetSize({ 1920, 1080 });
	// @param background_color Starting background color of the window. Can be changed later using
	// game.renderer.SetClearColor(color::Black);
	void Init(
		const std::string& title = "Default Title", const V2_int& window_size = { 800, 800 },
		const Color& background_color = color::White
	);

	template <typename TScene, typename TKey, typename... TArgs>
	void Start(
		const TKey& scene_key, const SceneTransition& transition = {}, TArgs&&... constructor_args
	) {
		scene.EnterStartScene<TScene>(
			scene_key, transition, std::forward<TArgs>(constructor_args)...
		);
	}

	// Stops the game from running.
	void Stop();

	// @return True if the game is running, false if it has been stopped.
	[[nodiscard]] bool IsRunning() const;

	// @return True if the game subsystems have been initialized, false otherwise.
	[[nodiscard]] bool IsInitialized() const;

private:
	friend struct WindowDeleter;
	friend struct Mix_MusicDeleter;
	friend struct Mix_ChunkDeleter;
	friend struct SDL_SurfaceDeleter;
	friend struct TTF_FontDeleter;
	friend class GLContext;
	friend class SceneManager;
	friend class RenderTarget;
#ifdef __EMSCRIPTEN__
	friend void EmscriptenLoop();
#endif

	void MainLoop();
	void Update();
	void Shutdown();

	// Note: To order of these is important for correct construction.

	std::unique_ptr<SDLInstance> sdl_instance_;
	std::unique_ptr<Window> window_;

public:
	Window& window;

private:
	std::unique_ptr<GLContext> gl_context_;
	std::unique_ptr<EventHandler> event_;

public:
	EventHandler& event;

private:
	std::unique_ptr<InputHandler> input_;

public:
	InputHandler& input;

private:
	std::unique_ptr<Renderer> renderer_;

public:
	Renderer& renderer;

private:
	std::unique_ptr<SceneManager> scene_;

public:
	SceneManager& scene;

private:
	std::unique_ptr<Physics> physics_;

public:
	Physics& physics;

private:
	std::unique_ptr<CollisionHandler> collision_;

public:
	CollisionHandler& collision;

private:
	std::unique_ptr<UserInterface> ui_;

public:
	UserInterface& ui;

private:
	std::unique_ptr<CameraManager> camera_;

public:
	CameraManager& camera;

private:
	std::unique_ptr<TweenManager> tween_;

public:
	TweenManager& tween;

private:
	std::unique_ptr<MusicManager> music_;

public:
	MusicManager& music;

private:
	std::unique_ptr<SoundManager> sound_;

public:
	SoundManager& sound;

private:
	std::unique_ptr<JsonManager> json_;

public:
	JsonManager& json;

private:
	std::unique_ptr<FontManager> font_;

public:
	FontManager& font;

private:
	// std::unique_ptr<TextManager> text_;

public:
	// TextManager& text;

private:
	std::unique_ptr<TextureManager> texture_;

public:
	TextureManager& texture;

private:
	std::unique_ptr<ShaderManager> shader_;

public:
	ShaderManager& shader;

private:
	// std::unique_ptr<LightManager> light_;

public:
	// LightManager& light;

private:
	std::unique_ptr<Profiler> profiler_;

public:
	Profiler& profiler;

private:
	bool running_{ false };
	float dt_{ 0.0f };

public:
#ifdef PTGN_DEBUG
	Stats stats;
#endif
};

} // namespace impl

extern impl::Game game;

} // namespace ptgn
