#include "game.h"

#include <cassert>

namespace ptgn {

namespace global {

namespace hidden {

std::unique_ptr<Game> game{ nullptr };

} // namespace hidden

void InitGame() {
	hidden::game = std::make_unique<Game>();
}

void DestroyGame() {
	hidden::game.reset();
}

Game& GetGame() {
	assert(hidden::game != nullptr && "Game not initialized?");
	return *hidden::game;
}

} // namespace global

} // namespace ptgn