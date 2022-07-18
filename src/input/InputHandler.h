#pragma once

#include <array> // std::array
#include <cstdlib> // std::size_t
#include <cstdint> // std::int64_t, etc
#include <tuple> // std::pair
#include <algorithm> // std::copy
#include <cassert> // assert

#include "input/Mouse.h"
#include "math/Vector2.h"
#include "utility/Timer.h"

namespace ptgn {

struct InputHandler {
	static InputHandler& Get() {
		static InputHandler instance;
		return instance;
	}
	// Enum for storing states of mouse keys.
	enum class MouseState {
		DOWN,
		PRESSED,
		UP,
		RELEASED
	};
	// Number of keys stored in the SDL key states array. For creating previous key states array.
	static constexpr const std::size_t KEY_COUNT{ 512 };

	// Updates previous key states for key up and down check.
	void InputHandler::UpdateKeyStates(const std::uint8_t* key_states) {
		// Copy current key states to previous key states.
		std::copy(key_states, key_states + KEY_COUNT, std::begin(previous_key_states_));
	}

	// Updates previous mouse states for mouse up and down check.
	void InputHandler::UpdateMouseState(Mouse button) {
		auto [state, timer] = GetMouseStateAndTimer(button);
		if (timer->IsRunning() && *state == MouseState::DOWN) {
			*state = MouseState::PRESSED;
		} else if (!timer->IsRunning() && *state == MouseState::UP) {
			*state = MouseState::RELEASED;
		}
	}

	/*
	* @param button Mouse enum corresponding to the desired button.
	* @return Pair of pointers to the mouse state and timer for a given button,
	* pair of nullptrs if no such button exists.
	*/
	std::pair<InputHandler::MouseState*, Timer*> InputHandler::GetMouseStateAndTimer(Mouse button) {
		switch (button) {
			case Mouse::LEFT:
				return { &left_mouse_, &left_mouse_timer_ };
			case Mouse::RIGHT:
				return { &right_mouse_, &right_mouse_timer_ };
			case Mouse::MIDDLE:
				return { &middle_mouse_, &middle_mouse_timer_ };
			default:
				break;
		}
		assert(!"Input handler cannot retrieve state and timer for invalid mouse button");
		return { nullptr, nullptr };
	}

	/*
	* @param button Mouse enum corresponding to the desired button.
	* @return Current state of the given mouse button.
	*/
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

	// Previous loop cycle key states for comparison with current.
	std::array<std::uint8_t, KEY_COUNT> previous_key_states_{};
private:
	// Mouse states.
	MouseState left_mouse_{ MouseState::RELEASED };
	MouseState right_mouse_{ MouseState::RELEASED };
	MouseState middle_mouse_{ MouseState::RELEASED };

	// Mouse button held for timers.

	Timer left_mouse_timer_;
	Timer right_mouse_timer_;
	Timer middle_mouse_timer_;
};

} // namespace ptgn