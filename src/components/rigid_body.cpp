#include "components/rigid_body.h"

#include "core/game.h"

namespace ptgn {

void RigidBody::Update() {
	velocity.y += gravity * game.dt();
	velocity   += acceleration * game.dt();
	velocity   *= 1.0f / (1.0f + drag * game.dt());
	// Or alternatively: velocity *= std::clamp(1.0f - drag * game.dt(), 0.0f, 1.0f);
	if (velocity.MagnitudeSquared() > max_velocity * max_velocity) {
		velocity = velocity.Normalized() * max_velocity;
	}
}

} // namespace ptgn