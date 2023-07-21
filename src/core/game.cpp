#include "game.h"

#include <cassert>

namespace ptgn {

namespace global {

namespace impl {

std::unique_ptr<Game> game{ nullptr };

} // namespace impl

void InitGame() {
	impl::game = std::make_unique<Game>();
}

void DestroyGame() {
	impl::game.reset();
}

Game& GetGame() {
	assert(impl::game != nullptr && "Game not initialized?");
	return *impl::game;
}

} // namespace global

} // namespace ptgn
