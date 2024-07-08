#pragma once

#include <memory>

#include "core/opengl_instance.h"
#include "core/sdl_instance.h"
#include "event/event_handler.h"
#include "event/input_handler.h"
#include "protegon/resources.h"
#include "protegon/scene.h"

namespace ptgn {

class Game {
public:
	Game()	= default;
	~Game() = default;

	void Loop();

	SDLInstance sdl;
	gl::OpenGLInstance opengl; // Must be initialized after SDL2.
	EventHandler event;
	ResourceManagers managers;
	SceneManager scene;
	InputHandler input;
};

namespace impl {

void InitializeFileSystem();

} // namespace impl

namespace global {

namespace impl {

extern std::unique_ptr<Game> game;

void InitGame();

} // namespace impl

[[nodiscard]] Game& GetGame();

} // namespace global

} // namespace ptgn