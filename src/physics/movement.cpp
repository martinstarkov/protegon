#include "physics/movement.h"

#include "core/game.h"
#include "event/input_handler.h"
#include "event/key.h"
#include "math/vector2.h"

namespace ptgn {

namespace impl {

void MoveImpl(
	V2_float& vel, const V2_float& amount, Key left_key, Key right_key, Key up_key, Key down_key,
	bool cancel_velocity_if_unpressed
) {
	bool left{ game.input.KeyPressed(left_key) };
	bool right{ game.input.KeyPressed(right_key) };
	bool up{ game.input.KeyPressed(up_key) };
	bool down{ game.input.KeyPressed(down_key) };
	if (left && !right) {
		vel.x -= amount.x;
	} else if (right && !left) {
		vel.x += amount.x;
	}
	if (up && !down) {
		vel.y -= amount.y;
	} else if (down && !up) {
		vel.y += amount.y;
	}
	if (cancel_velocity_if_unpressed && !up && !down && !left && !right) {
		vel = {};
	}
}

} // namespace impl

void MoveWASD(V2_float& vel, const V2_float& amount, bool cancel_velocity_if_unpressed) {
	impl::MoveImpl(vel, amount, Key::A, Key::D, Key::W, Key::S, cancel_velocity_if_unpressed);
}

void MoveArrowKeys(V2_float& vel, const V2_float& amount, bool cancel_velocity_if_unpressed) {
	impl::MoveImpl(
		vel, amount, Key::LEFT, Key::RIGHT, Key::UP, Key::DOWN, cancel_velocity_if_unpressed
	);
}

} // namespace ptgn
