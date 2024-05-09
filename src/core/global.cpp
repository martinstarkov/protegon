#include "global.h"

namespace ptgn {

namespace global {

namespace impl {

// TODO: Figure out a way to do this without raw pointers.
// Shared_ptr does not work here because this is a global variable so it would not go out of scope when Game does.
Game* game{ nullptr };

} // namespace impl

Game& GetGame() {
	assert(impl::game != nullptr && "Game not initialized or destroyed early");
	return *impl::game;
}

} // namespace global

} // namespace ptgn