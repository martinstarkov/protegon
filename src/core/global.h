#pragma once

#include "protegon/game.h"

#include "game_systems.h"

namespace ptgn {

class Game {
public:
	Game();
	~Game();
	// Override function for when engine is created.
	// Usually this is where one would load fonts and scenes into the respective resource managers.
	virtual void Create();
	virtual void Update(float dt);

	void Stop();
private:
	bool running_{ false };
	void Loop();
};

namespace global {

namespace impl {

extern Game* game;

} // namespace impl

Game& GetGame();

} // namespace global

} // namespace ptgn