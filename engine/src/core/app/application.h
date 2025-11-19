#pragma once

// #include "core/event/event_handler.h"
#include "core/app/window.h"
#include "core/asset/asset_manager.h"
#include "core/input/input_handler.h"
#include "debug/debug_system.h"
#include "math/vector2.h"
#include "renderer/renderer.h"
#include "scene/scene_manager.h"

namespace ptgn {

#ifdef __EMSCRIPTEN__

namespace impl {

void EmscriptenMainLoop(void* application);

} // namespace impl

#endif

struct ApplicationConfig {
	const char* title{ "Default Title" };
	V2_int window_size{ 800, 800 };
};

class Application {
public:
	explicit Application(const ApplicationConfig& config = {});
	~Application() noexcept						   = default;
	Application(const Application&)				   = delete;
	Application& operator=(const Application&)	   = delete;
	Application(Application&&) noexcept			   = delete;
	Application& operator=(Application&&) noexcept = delete;

	template <typename TScene, typename TransitionIn, class... TArgs>
		requires std::constructible_from<TScene, TArgs...>
	void StartWith(std::string_view scene_key, TransitionIn&& transition_in, TArgs&&... args) {
		// Initialize the first scene using the SceneManager.
		scenes_.SwitchTo<TScene>(
			scene_key,
			std::make_unique<std::remove_cvref_t<TransitionIn>>(
				std::forward<TransitionIn>(transition_in)
			),
			std::forward<TArgs>(args)...
		);

		// Flush queued ops so the first scene becomes active before main loop.
		scenes_.Update(0.0);

		EnterMainLoop();
	}

	// @return True if inside the main loop, false otherwise.
	[[nodiscard]] bool IsRunning() const;

	void Stop();

	// TODO: Move these elsewhere.
	float dt() const;
	float time() const;

private:
#ifdef __EMSCRIPTEN__
	friend void impl::EmscriptenMainLoop(void* application);
#endif

	struct SDLInstance {
		SDLInstance();
		~SDLInstance() noexcept;
		SDLInstance(const SDLInstance&)				   = delete;
		SDLInstance& operator=(const SDLInstance&)	   = delete;
		SDLInstance(SDLInstance&&) noexcept			   = delete;
		SDLInstance& operator=(SDLInstance&&) noexcept = delete;
	};

	SDLInstance sdl_;

	Window window_;
	Renderer renderer_;
	SceneManager scenes_;
	InputHandler input_;

	AssetManager assets_;

	// TODO: Make a no-op version of this for release modes.
	impl::DebugSystem debug_;

	// EventRegistry events_;

	void EnterMainLoop();
	void Update();

	float dt_{ 0.0f };
	bool running_{ false };
};

} // namespace ptgn