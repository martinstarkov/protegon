#pragma once

#include <memory> // std::unique_ptr

#include "core/sdl_instance.h"
#include "protegon/resources.h"
#include "protegon/scene.h"
#include "event/input_handler.h"

namespace ptgn {

struct Game {
	SDLInstance sdl;
	ResourceManagers managers;
	InputHandler input;
	SceneManager scene;
	bool running{ false };
};

namespace global {

namespace impl {

extern std::unique_ptr<Game> game;

} // namespace impl

void InitGame();
void DestroyGame();
Game& GetGame();

} // namespace global

} // namespace ptgn