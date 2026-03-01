#pragma once

#include <concepts>
#include <memory>
#include <string_view>
#include <type_traits>

#include "core/math/vector2.h"
#include "core/time/time.h"
#include "platform/input/input_handler.h"
#include "platform/window/window.h"
#include "renderer/renderer.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/asset/font_system.h"
#include "runtime/audio/audio_system.h"
#include "runtime/event/event_handler.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_manager.h"
#include "tools/debug/debug_system.h"

namespace ptgn {

class Application;
class ApplicationContext;

namespace impl {

#ifdef __EMSCRIPTEN__

void EmscriptenMainLoop(void* application);

#endif

// TODO: Move this class elsewhere so entire Application.h does not need to be included when
// subsystems use sdl.
class SDLInstance {
private:
	friend class ptgn::Application;

	SDLInstance();
	~SDLInstance() noexcept;
	SDLInstance(const SDLInstance&)				   = delete;
	SDLInstance& operator=(const SDLInstance&)	   = delete;
	SDLInstance(SDLInstance&&) noexcept			   = delete;
	SDLInstance& operator=(SDLInstance&&) noexcept = delete;
};

} // namespace impl

/// @brief Configuration data used to initialize an Application.
struct ApplicationConfig {
	WindowConfig window;
};

/// @brief Core engine entry point coordinating windowing, rendering,
///        input, scenes, assets, and the main loop.
///
/// Owns all major subsystems and drives the frame update loop.
class Application {
public:
	/// @brief Constructs the application using the provided configuration.
	/// @param config Application initialization settings (window, etc.).
	explicit Application(const ApplicationConfig& config = {});
	explicit Application(const std::string& title);
	explicit Application(const std::string& title, V2_int window_size);

	~Application() noexcept;
	Application(const Application&)				   = delete;
	Application& operator=(const Application&)	   = delete;
	Application(Application&&) noexcept			   = delete;
	Application& operator=(Application&&) noexcept = delete;

	/// @brief Starts the application with the specified initial scene.
	///
	/// @tparam TScene Scene type to instantiate.
	/// @param scene_key Unique identifier for the scene instance.
	/// @param args Arguments forwarded to the scene constructor.
	template <SceneType TScene, typename... TArgs>
		requires std::constructible_from<TScene, TArgs...>
	void StartWith(std::string_view scene_key, TArgs&&... args) {
		renderer_.UpdateDisplayViewport(window_.GetSize(), false);

		// Initialize the first scene using the SceneManager.
		scenes_.SwitchTo<TScene>(scene_key, nullptr, std::forward<TArgs>(args)...);

		// Flush queued ops so the first scene becomes active before main loop.
		scenes_.Update(secondsf{ 0.0f }, false);

		EnterMainLoop();
	}

	template <SceneType TScene>
		requires std::is_default_constructible_v<TScene>
	void StartWith() {
		StartWith<TScene>("");
	}

private:
#ifdef __EMSCRIPTEN__
	friend void impl::EmscriptenMainLoop(void* application);
#endif
	friend class ApplicationContext;

	impl::SDLInstance sdl_;

	Window window_;
	Renderer renderer_;
	EventHandler events_;
	SceneManager scenes_;
	InputHandler input_;
	AssetManager assets_;
	FontSystem font_;
	AudioSystem audio_;

	// TODO: Make a no-op version of this for release modes.
	impl::DebugSystem debug_;

	void EnterMainLoop();
	void Update();
	[[nodiscard]] milliseconds TimeSinceStart() const;

	secondsf dt_{ 0.0f };
	bool running_{ false };
	std::size_t frame_count_{ 0 };

	std::shared_ptr<ApplicationContext> ctx_;
};

} // namespace ptgn