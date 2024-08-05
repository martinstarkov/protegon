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
		Loop();
	}

	void Stop();

private:
	void Reset();

	void Loop();
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

		Update(loop_function, condition);

		if constexpr (!is_window_quit) {
			dispatcher.Unsubscribe((void*)&condition);
		}
	}

	bool running_{ false };
};

extern Game game;

} // namespace ptgn