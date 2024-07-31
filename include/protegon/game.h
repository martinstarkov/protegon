#pragma once

#include <chrono>
#include <memory>
#include <variant>

#include "core/resource_managers.h"
#include "core/window.h"
#include "event/event_handler.h"
#include "event/input_handler.h"
#include "renderer/renderer.h"
#include "scene/scene_manager.h"
#include "utility/profiling.h"

namespace ptgn {

class Game;

namespace impl {

class GameInstance {
public:
	GameInstance(Game& g);
	~GameInstance();
};

} // namespace impl

class Game {
public:
	Game()	= default;
	~Game() = default;

private:
	Game(const Game&)			 = delete;
	Game(Game&&)				 = default;
	Game& operator=(const Game&) = delete;
	Game& operator=(Game&&)		 = default;

public:
	using UpdateFunction = std::variant<std::function<void()>, std::function<void(float dt)>>;

	void LoopUntilKeyDown(const std::vector<Key>& any_of_keys, const UpdateFunction& loop_function);
	void LoopUntilQuit(const UpdateFunction& loop_function);

	// Optional: pass in constructor arguments for the TStartScene.
	template <typename TStartScene, typename... TArgs>
	void Start(TArgs&&... constructor_args) {
		Init();
		scene.StartScene<TStartScene>(
			impl::start_scene_key, std::forward<TArgs>(constructor_args)...
		);
		SceneLoop();
	}

	void Stop();

	// Systems

	InputHandler input;
	Window window;
	Renderer renderer;
	EventHandler event;
	SceneManager scene;

	// Resources

	MusicManager music;
	SoundManager sound;
	FontManager font;
	TextManager text;
	TextureManager texture;
	ShaderManager shader;

	// Debug

	Profiler profiler;

private:
	void Init();
	void SceneLoop();
	void Update(const UpdateFunction& loop_function);

	template <typename EventEnum, typename EventType>
	void LoopUntilEvent(
		EventDispatcher<EventEnum>& dispatcher, const std::vector<EventEnum>& events,
		const std::function<bool(const EventType&)> exit_condition_function,
		const UpdateFunction& loop_function
	) {
		bool running = true;

		constexpr bool is_window_quit{ std::is_same_v<EventType, WindowQuitEvent> };

		if constexpr (!is_window_quit) {
			for (const EventEnum& event_enum : events) {
				dispatcher.Subscribe(
					event_enum, (void*)&running, std::function([&](const EventType& e) {
						if (exit_condition_function(e)) {
							running = false;
						}
					})
				);
			}
		}

		// Always quit on window quit.
		event.window.Subscribe(
			WindowEvent::Quit, (void*)&running,
			std::function([&](const WindowQuitEvent& e) { running = false; })
		);

		// Optional: Update window while it is being dragged. Upside: No rendering artefacts;
		// Downside: window dragging becomes laggier. If enabling this, it is adviseable to change
		// Renderer::Init such that the renderer viewport is updated during window resizing instead
		// of after it has been resized.
		/*event.window.Subscribe(
			WindowEvent::Drag, (void*)this,
			std::function([&](const WindowDragEvent& e) { update_function(); })
		);*/

		while (running && instance_ != nullptr) {
			Update(loop_function);
		}

		// Important to clear previous info from input cache (e.g. first time key presses).
		// Otherwise they might trigger again in the next input.Update().
		input.Reset();

		if constexpr (!is_window_quit) {
			dispatcher.Unsubscribe((void*)&running);
		}
		event.window.Unsubscribe((void*)&running);
	}

	std::unique_ptr<impl::GameInstance> instance_;
};

extern Game game;

} // namespace ptgn