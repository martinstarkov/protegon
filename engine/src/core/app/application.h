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
private:
	inline static Application* instance_{ nullptr };

	struct SDLInstance {
		SDLInstance();
		~SDLInstance();
		SDLInstance(const SDLInstance&)			   = delete;
		SDLInstance& operator=(const SDLInstance&) = delete;
		SDLInstance(SDLInstance&&)				   = delete;
		SDLInstance& operator=(SDLInstance&&)	   = delete;
	};

	SDLInstance sdl_;

public:
	explicit Application(const ApplicationConfig& config = {});
	~Application() noexcept;
	Application(const Application&)				   = delete;
	Application& operator=(const Application&)	   = delete;
	Application(Application&&) noexcept			   = delete;
	Application& operator=(Application&&) noexcept = delete;

	// template <impl::SceneType TScene, impl::SceneTransitionType TransitionIn, class... TArgs>
	//	requires std::constructible_from<TScene, TArgs...>
	// void StartWith(const SceneKey& key, TransitionIn&& transition_in, TArgs&&... args) {
	//	// Construct and register the scene
	//	scene_.Transition<TScene>(
	//		std::nullopt, key, std::forward<TransitionIn>(transition_in), NoTransition{},
	//		std::forward<TArgs>(args)...
	//	);
	//	EnterMainLoop();
	// }

	template <typename TScene, typename TransitionIn, class... TArgs>
		requires std::constructible_from<TScene, TArgs...>
	void StartWith(const SceneKey& key, TransitionIn&& transition_in, TArgs&&... args) {
		// Initialize the first scene using the SceneManager.
		scene_.SwitchTo<TScene>(
			key,
			std::make_unique<std::remove_cvref_t<TransitionIn>>(
				std::forward<TransitionIn>(transition_in)
			),
			std::forward<TArgs>(args)...
		);

		// Flush queued ops so the first scene becomes active before main loop.
		scene_.Update(0.0);

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
	Window window_;
	// TODO: Move this to private.
	InputHandler input_;

	// TODO: Merge these all into asset manager and make it private.
	impl::JsonManager json;
	impl::SoundManager sound;
	impl::MusicManager music;
	impl::TextureManager texture;
	impl::ShaderManager shader;
	impl::FontManager font;

	// TODO: Move this to private.
	Renderer render_;

	// TODO: Move this to private.
	SceneManager scene_;

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
	// TODO: Add this.
	// EventRegistry events_;
	// AssetManager assets_;

	float dt_{ 0.0f };
	bool running_{ false };
};

} // namespace ptgn