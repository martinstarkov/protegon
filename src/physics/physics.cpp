#include "physics/physics.h"

#include "core/game.h"

namespace ptgn {

namespace impl {

V2_float Physics::GetGravity() const {
	return gravity_;
}

void Physics::SetGravity(const V2_float& gravity) {
	gravity_ = gravity;
}

float Physics::dt() const {
	// TODO: Consider changing in the future.
	return game.dt();
}

} // namespace impl

} // namespace ptgn
