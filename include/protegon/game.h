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
	void RepeatUntilQuit(UpdateFunction while_not_quit);

	template <typename TStartScene, typename... TArgs>
	// Optional: pass in constructor arguments for the TStartScene.
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
		RepeatUntilQuit([&](float dt) {
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

	// Systems:

	InputHandler input;
	Window window;
	Renderer renderer;
	EventHandler event;
	SceneManager scene;

	// Resources:

	MusicManager music;
	SoundManager sound;
	FontManager font;
	TextManager text;
	TextureManager texture;
	ShaderManager shader;

private:
	std::unique_ptr<impl::GameInstance> instance_;
};

extern Game game;

} // namespace ptgn