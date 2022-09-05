#include "input_handler.h"

#include <cassert>   // assert
#include <algorithm> // std::copy

namespace ptgn {

void InputHandler::UpdateKeyStates(const std::uint8_t* key_states) {
	// Copy current key states to previous key states.
	std::copy(key_states, key_states + KEY_COUNT, std::begin(previous_key_states_));
}

void InputHandler::UpdateMouseState(Mouse button) {
	auto [state, timer] = GetMouseStateAndTimer(button);
	if (timer->IsRunning() && *state == MouseState::DOWN) {
		*state = MouseState::PRESSED;
	} else if (!timer->IsRunning() && *state == MouseState::UP) {
		*state = MouseState::RELEASED;
	}
}

std::pair<InputHandler::MouseState&, Timer&> InputHandler::GetMouseStateAndTimer(Mouse button) {
	switch (button) {
		case Mouse::LEFT:
			return { left_mouse_, left_mouse_timer_ };
		case Mouse::RIGHT:
			return { right_mouse_, right_mouse_timer_ };
		case Mouse::MIDDLE:
			return { middle_mouse_, middle_mouse_timer_ };
	}
	assert(!"Input handler cannot retrieve state and timer for invalid mouse button");;
}

InputHandler::MouseState InputHandler::GetMouseState(Mouse button) const {
	switch (button) {
		case Mouse::LEFT:
			return left_mouse_;
		case Mouse::RIGHT:
			return right_mouse_;
		case Mouse::MIDDLE:
			return middle_mouse_;
		default:
			return left_mouse_;
	}
}

} // namespace ptgn