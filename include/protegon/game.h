#pragma once

#include <memory>
#include <variant>

#include "core/gl_context.h"
#include "core/resource_managers.h"
#include "core/sdl_instance.h"
#include "core/window.h"
#include "event/event_handler.h"
#include "event/input_handler.h"
#include "renderer/renderer.h"
#include "scene/scene_manager.h"
#include "utility/profiling.h"

namespace ptgn {

class Game {
public:
	Game();
	~Game() = default;

private:
	Game(const Game&)			 = delete;
	Game(Game&&)				 = delete;
	Game& operator=(const Game&) = delete;
	Game& operator=(Game&&)		 = delete;

private:
	impl::SDLInstance sdl_instance_;

public:
	Window window;

private:
	impl::GLContext gl_context_;

public:
	// Core Subsystems

	EventHandler event;
	InputHandler input;
	Renderer renderer;
	SceneManager scene;
	ActiveSceneCameraManager camera;

	// Resources

	TweenManager tween;
	MusicManager music;
	SoundManager sound;
	FontManager font;
	TextManager text;
	TextureManager texture;
	ShaderManager shader;

	// Debug

	Profiler profiler;

public:
	using UpdateFunction = std::variant<std::function<void()>, std::function<void(float dt)>>;

	void LoopUntilKeyDown(const std::vector<Key>& any_of_keys, const UpdateFunction& loop_function);
	void LoopUntilQuit(const UpdateFunction& loop_function);

	// Optional: pass in constructor arguments for the TStartScene.
	template <typename TStartScene, typename... TArgs>
	void Start(TArgs&&... constructor_args) {
		running_ = true;
		scene.StartScene<TStartScene>(
			impl::start_scene_key, std::forward<TArgs>(constructor_args)...
		);
		// Design decision: Latest possible point to show window is right before
		// loop starts. Comment this if you wish the window to appear hidden for an
		// indefinite period of time.
		window.Show();
		while (running_) {
			MainLoop();
		}
		Reset();
	}

	void Stop();

private:
	void Reset();

	void MainLoop();
	void LoopFunction(const UpdateFunction& loop_function);
	void Update(const UpdateFunction& loop_function, int& condition);

	template <typename EventEnum, typename EventType>
	void LoopUntilEvent(
		EventDispatcher<EventEnum>& dispatcher, const std::vector<EventEnum>& events,
		const std::function<bool(const EventType&)> exit_condition_function,
		const UpdateFunction& loop_function
	) {
		int condition = true;

		constexpr bool is_window_quit{ std::is_same_v<EventType, WindowQuitEvent> };

		if constexpr (!is_window_quit) {
			for (const EventEnum& event_enum : events) {
				dispatcher.Subscribe(
					event_enum, (void*)&condition, std::function([&](const EventType& e) {
						if (exit_condition_function(e)) {
							condition = false;
						}
					})
				);
			}
		}

		// Always quit on window quit.
		event.window.Subscribe(
			WindowEvent::Quit, (void*)&condition,
			std::function([&](const WindowQuitEvent& e) { condition = false; })
		);

		// Optional: Update window while it is being dragged. Upside: No rendering artefacts;
		// Downside: window dragging becomes laggier. If enabling this, it is adviseable to change
		// Renderer constructor such that the renderer viewport is updated during window resizing
		// instead of after it has been resized.
		/*event.window.Subscribe(
			WindowEvent::Drag, (void*)&condition,
			std::function([&](const WindowDragEvent& e) { loop_function(); })
		);*/

		std::size_t counter{ 0 };
		auto start{ std::chrono::system_clock::now() };
		auto end{ std::chrono::system_clock::now() };

		while (running_ && condition) {
			LoopFunction(loop_function);
		}

		// Important to clear previous info from input cache (e.g. first time key presses).
		// Otherwise they might trigger again in the next input.Update().
		input.Reset();

		event.window.Unsubscribe((void*)&condition);

		if constexpr (!is_window_quit) {
			dispatcher.Unsubscribe((void*)&condition);
		}
	}

	bool running_{ false };
};

extern Game game;

} // namespace ptgn