#pragma once

#include <memory>
#include <string>

#include "core/app/window.h"
#include "core/asset/asset_manager.h"
// #include "core/event/event_handler.h"
#include "audio/audio.h"
#include "core/input/input_handler.h"
#include "debug/runtime/debug_system.h"
#include "math/vector2.h"
#include "renderer/material/shader.h"
#include "renderer/renderer.h"
#include "serialization/json/json_manager.h"
#include "world/scene/scene_key.h"
#include "world/scene/scene_manager.h"

namespace ptgn {

namespace impl {

class ProfileInstance;

#ifdef __EMSCRIPTEN__

void EmscriptenMainLoop(void* application);

#endif

} // namespace impl

struct ApplicationConfig {
	const char* title{ "Default Title" };
	V2_int window_size{ 800, 800 };
};

class Application {
public:
	Application(const ApplicationConfig& config = {});
	~Application();
	Application(const Application&)			   = delete;
	Application& operator=(const Application&) = delete;
	Application(Application&&)				   = delete;
	Application& operator=(Application&&)	   = delete;

	template <impl::SceneType TScene, impl::SceneTransitionType TransitionIn, class... TArgs>
		requires std::constructible_from<TScene, TArgs...>
	void StartWith(const SceneKey& key, TransitionIn&& transition_in, TArgs&&... args) {
		// Construct and register the scene
		scene_.Transition<TScene>(
			std::nullopt, key, std::forward<TransitionIn>(transition_in), NoTransition{},
			std::forward<TArgs>(args)...
		);
		EnterMainLoop();
	}

	// @return True if inside the main loop, false otherwise.
	[[nodiscard]] bool IsRunning() const;

	void Stop();

	// TODO: Move these elsewhere.

	float dt() const;

	float time() const;

	// TODO: Move this to private.
	impl::DebugSystem debug_;
	// TODO: Move this to private.
	static Application& Get();
	// TODO: Move this to private.
	Renderer render_;
	// TODO: Move this to private.
	Window window_;
	// TODO: Move this to private.
	SceneManager scene_;
	// TODO: Move this to private.
	InputHandler input_;

	// TODO: Merge these all into asset manager and make it private.
	impl::JsonManager json;
	impl::SoundManager sound;
	impl::MusicManager music;
	impl::TextureManager texture;
	impl::FontManager font;
	impl::ShaderManager shader;

private:
#ifdef __EMSCRIPTEN__

	friend void impl::EmscriptenMainLoop(void* application);

#endif
	friend class impl::ProfileInstance;
	// TODO: Get rid of this.
	friend class InputHandler;
	// TODO: Get rid of this.
	friend class Scene;
	// TODO: Get rid of this.
	friend class impl::DebugSystem;

	void EnterMainLoop();
	void Update();

	struct SDLInstance {
		SDLInstance();
		~SDLInstance();
		SDLInstance(const SDLInstance&)			   = delete;
		SDLInstance& operator=(const SDLInstance&) = delete;
		SDLInstance(SDLInstance&&)				   = delete;
		SDLInstance& operator=(SDLInstance&&)	   = delete;
	};

	SDLInstance sdl_;
	// TODO: Add this.
	// EventRegistry events_;
	// AssetManager assets_;

	float dt_{ 0.0f };
	bool running_{ false };

	inline static Application* instance_{ nullptr };
};

} // namespace ptgn