#pragma once

#include <memory> // std::unique_ptr

#include "core/sdl_instance.h"
#include "protegon/resources.h"

namespace ptgn {

struct Game {
	SDLInstance sdl;
	ResourceManagers managers;
	// TODO: Add InputHandler input;
};

namespace global {

namespace hidden {

extern std::unique_ptr<Game> game;

} // namespace hidden

void InitGame();
void DestroyGame();
Game& GetGame();

} // namespace global

} // namespace ptgn