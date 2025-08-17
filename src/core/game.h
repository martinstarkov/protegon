#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "debug/stats.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "utility/file.h"

namespace ptgn {

// TODO: Add shader loading support (.VERT + .FRAG)
//
// Load various different resource types from a json file. Json format must be:
// {
//    "resource_key": "path/to/resource/file.extension",
//    ...
// }
// Supported extensions:
// Texture: .PNG, .JPG, .BMP, .GIF
// Audio: .OGG (only one supported by Emscripten), MP3, WAV, OPUS
// Font: .TTF JSON: .JSON
// @param music_resource_suffix Resources which have a key that ends with this suffix will be loaded
// as music instead of sounds, provided their extension is a valid audio extension.
void LoadResources(const path& resource_file, std::string_view music_resource_suffix = "_music");

// @param is_music If true and is a supported audio format, loads the resource as music instead of
// sound.
// Resource path must end in a supported extension (see LoadResources comment).
void LoadResource(std::string_view key, const path& resource_path, bool is_music = false);

struct Resource {
	std::string_view key;
	path filepath;
	// @param is_music If true and is a supported audio format, loads the resource as music instead
	// of sound.
	bool is_music{ false };
};

void LoadResources(const std::vector<Resource>& resource_paths);

namespace impl {

class SDLInstance;
class GLContext;
class Window;
class InputHandler;
class Renderer;
class SceneManager;
class JsonManager;
class MusicManager;
class SoundManager;
class FontManager;
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
	// @return Previous frame time in seconds.
	[[nodiscard]] float dt() const;

	// @return Milliseconds since Init() was called.
	[[nodiscard]] float time() const;

	// Entry point for the game / application.
	// Note: Window will not appear until an active scene has been loaded into the scene manager:
	// game.scene.Enter<MyScene>("scene_name");
	// @param title The title of the window. Can be changed later using
	// game.window.SetTitle("title");
	// @param logical_resolution Logical resolution of the game. Can be changed later using
	// game.renderer.SetLogicalResolution({ 1920, 1080 }); Note: By default this will also be set as
	// the window size, however, the window may be resized later.
	// @param background_color Starting background color of the window. Can be changed later using
	// game.renderer.SetClearColor(color::Black);
	void Init(
		const std::string& title = "Default Title", const V2_int& logical_resolution = { 800, 800 },
		const Color& background_color = color::Transparent
	);

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
	friend class FontManager;
	friend class GLContext;
	friend class SceneManager;
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
	std::unique_ptr<TextureManager> texture_;

public:
	TextureManager& texture;

private:
	std::unique_ptr<ShaderManager> shader_;

public:
	ShaderManager& shader;

private:
	std::unique_ptr<Profiler> profiler_;

public:
	Profiler& profiler;

private:
	bool running_{ false };
	// Frame time in seconds.
	float dt_{ 0.0f };

public:
#ifdef PTGN_DEBUG
	Stats stats;
#endif
};

} // namespace impl

extern impl::Game game;

} // namespace ptgn