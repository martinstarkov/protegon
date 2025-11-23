#pragma once

#include <concepts>
#include <memory>
#include <string_view>

#include "core/util/time.h"
#include "math/vector2.h"
#include "scene/scene_manager.h"

namespace ptgn {

class ApplicationContext;
class Window;
class Renderer;
class SceneManager;
class EventHandler;
class InputHandler;
class AssetManager;

namespace impl {

class DebugSystem;

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
	explicit Application(const ApplicationConfig& config = {});
	~Application() noexcept;
	Application(const Application&)				   = delete;
	Application& operator=(const Application&)	   = delete;
	Application(Application&&) noexcept			   = delete;
	Application& operator=(Application&&) noexcept = delete;

	template <typename TScene, typename... TArgs>
		requires std::constructible_from<TScene, TArgs...>
	void StartWith(std::string_view scene_key, TArgs&&... args) {
		// Initialize the first scene using the SceneManager.
		scenes_->SwitchTo<TScene>(scene_key, nullptr, std::forward<TArgs>(args)...);

		// Flush queued ops so the first scene becomes active before main loop.
		scenes_->Update(secondsf{ 0.0f });

		EnterMainLoop();
	}

private:
#ifdef __EMSCRIPTEN__
	friend void impl::EmscriptenMainLoop(void* application);
#endif
	friend class ApplicationContext;

	struct SDLInstance {
		SDLInstance();
		~SDLInstance() noexcept;
		SDLInstance(const SDLInstance&)				   = delete;
		SDLInstance& operator=(const SDLInstance&)	   = delete;
		SDLInstance(SDLInstance&&) noexcept			   = delete;
		SDLInstance& operator=(SDLInstance&&) noexcept = delete;
	};

	SDLInstance sdl_;

	std::unique_ptr<Window> window_;
	std::unique_ptr<Renderer> renderer_;
	std::unique_ptr<SceneManager> scenes_;

	std::unique_ptr<EventHandler> events_;

	std::unique_ptr<InputHandler> input_;

	std::unique_ptr<AssetManager> assets_;

	// TODO: Make a no-op version of this for release modes.
	std::unique_ptr<impl::DebugSystem> debug_;

	void EnterMainLoop();
	void Update();

	secondsf dt_{ 0.0f };
	bool running_{ false };

	std::shared_ptr<ApplicationContext> ctx_;
};

} // namespace ptgn