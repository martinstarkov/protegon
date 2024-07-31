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

	template <typename EventEnum, typename EventType>
	void LoopUntilEvent(
		EventDispatcher<EventEnum>& dispatcher, const std::vector<EventEnum>& events,
		const std::function<bool(const EventType&)> exit_condition_function,
		const UpdateFunction& loop_function
	) {
		bool running = true;

		std::size_t counter = 0;
		using time			= std::chrono::time_point<std::chrono::system_clock>;
		time start{ std::chrono::system_clock::now() };
		time end{ std::chrono::system_clock::now() };

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

		auto update_function = [&]() {
			// Calculate time elapsed during previous frame.
			end = std::chrono::system_clock::now();
			duration<float> elapsed{ end - start };
			float dt{ elapsed.count() };
			start = end;

			input.Update();
			// For debugging:
			// PTGN_LOG("Updating ", counter);

			if (std::holds_alternative<std::function<void(float)>>(loop_function)) {
				std::get<std::function<void(float)>>(loop_function)(dt);
			} else {
				std::get<std::function<void(void)>>(loop_function)();
			}
			++counter;
		};

		// Optional: Update window while it is being dragged. Upside: No rendering artefacts;
		// Downside: window dragging becomes laggier. If enabling this, it is adviseable to change
		// Renderer::Init such that the renderer viewport is updated during window resizing instead
		// of after it has been resized.
		/*event.window.Subscribe(
			WindowEvent::Drag, (void*)this,
			std::function([&](const WindowDragEvent& e) { update_function(); })
		);*/

		while (running && instance_ != nullptr) {
			update_function();
		}

		// Important to clear previous info from input cache (e.g. first time key presses).
		// Otherwise they might trigger again in the next input.Update().
		input.Reset();

		if constexpr (!is_window_quit) {
			dispatcher.Unsubscribe((void*)&running);
		}
		event.window.Unsubscribe((void*)&running);
	}

	void LoopUntilKeyDown(const std::vector<Key>& any_of_keys, const UpdateFunction& loop_function);
	void LoopUntilQuit(const UpdateFunction& loop_function);

	// Optional: pass in constructor arguments for the TStartScene.
	template <typename TStartScene, typename... TArgs>
	void Start(TArgs&&... constructor_args) {
		static_assert(
			std::is_constructible_v<TStartScene, TArgs...>,
			"Start scene must be constructible from given arguments, check that start scene "
			"constructor is not private"
		);
		static_assert(
			std::is_convertible_v<TStartScene*, Scene*>, "Start scene must inherit from ptgn::Scene"
		);
		// Recall default constructor for all members.
		*this	  = {};
		instance_ = std::make_unique<impl::GameInstance>(*this);
		PTGN_ASSERT(!scene.Has(impl::start_scene_key), "Cannot load more than one start scene");
		// This may be unintuitive order but since the starting scene may set other
		// active scenes, it is important to set it first so it is the "earliest"
		// active scene in the list.
		scene.SetActive(impl::start_scene_key);
		scene.LoadStartScene<TStartScene>(
			impl::start_scene_key, std::forward<TArgs>(constructor_args)...
		);
		// In case Stop() was called in Scene constructor (non-looping scene).
		if (instance_ == nullptr) {
			return;
		}
		// Design decision: Latest possible point to show window is right before
		// loop starts. Comment this if you wish the window to appear hidden for an
		// indefinite period of time.
		window.Show();
		LoopUntilQuit([&](float dt) {
			renderer.Clear();
			scene.Update(dt);
			if (instance_ == nullptr) {
				return;
			}
			renderer.Present();
		});
		Stop();
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
	std::unique_ptr<impl::GameInstance> instance_;
};

extern Game game;

} // namespace ptgn