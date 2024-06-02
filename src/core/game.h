#pragma once

#include <memory>

#include "core/sdl_instance.h"
#include "core/opengl_instance.h"
#include "protegon/resources.h"
#include "protegon/scene.h"
#include "event/input_handler.h"
#include "event/event_handler.h"

namespace ptgn {

class Game {
public:
	Game() = default;
	~Game() = default;

	void Stop();
	void Loop();

	SDLInstance sdl;
	OpenGLInstance opengl; // Must be initialized after SDL2.
	EventHandler event;
	ResourceManagers managers;
	SceneManager scene;
	InputHandler input;
private:
	bool running_{ false };
};

namespace impl {

void InitializeFileSystem();

} // namespace impl

namespace global {

namespace impl {

extern std::unique_ptr<Game> game;

void InitGame();

} // namespace impl

Game& GetGame();

} // namespace global

} // namespace ptgn