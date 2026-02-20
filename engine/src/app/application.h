#pragma once

#include <concepts>
#include <memory>
#include <string_view>

#include "core/time/time.h"
#include "platform/input/input_handler.h"
#include "platform/window/window.h"
#include "renderer/renderer.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/event/event_handler.h"
#include "runtime/scene/scene_manager.h"
#include "tools/debug/debug_system.h"

struct MIX_Mixer;

namespace ptgn {

class ApplicationContext;

namespace impl {

#ifdef __EMSCRIPTEN__

void EmscriptenMainLoop(void* application);

#endif

struct SDLInstance {
	SDLInstance();
	~SDLInstance() noexcept;
	SDLInstance(const SDLInstance&)				   = delete;
	SDLInstance& operator=(const SDLInstance&)	   = delete;
	SDLInstance(SDLInstance&&) noexcept			   = delete;
	SDLInstance& operator=(SDLInstance&&) noexcept = delete;

	MIX_Mixer* mixer_{ nullptr };
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

	~Application() noexcept						   = default;
	Application(const Application&)				   = delete;
	Application& operator=(const Application&)	   = delete;
	Application(Application&&) noexcept			   = delete;
	Application& operator=(Application&&) noexcept = delete;

	/// @brief Starts the application with the specified initial scene.
	///
	/// @tparam TScene Scene type to instantiate.
	/// @param scene_key Unique identifier for the scene instance.
	/// @param args Arguments forwarded to the scene constructor.
	template <typename TScene, typename... TArgs>
		requires std::constructible_from<TScene, TArgs...>
	void StartWith(std::string_view scene_key, TArgs&&... args) {
		renderer_.UpdateDisplayViewport(window_.GetSize(), false);

		// Initialize the first scene using the SceneManager.
		scenes_.SwitchTo<TScene>(scene_key, nullptr, std::forward<TArgs>(args)...);

		// Flush queued ops so the first scene becomes active before main loop.
		scenes_.Update(secondsf{ 0.0f }, false);

		EnterMainLoop();
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
	InputHandler input_;
	SceneManager scenes_;
	AssetManager assets_;

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