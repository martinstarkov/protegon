#pragma once

#include <array>   // std::array
#include <cstdint> // std::uint8_t
#include <cstdlib> // std::size_t
#include <tuple>   // std::pair

#include "protegon/mouse.h"
#include "protegon/key.h"
#include "protegon/vector2.h"
#include "protegon/timer.h"

namespace ptgn {

struct InputHandler {
	// Enum for storing states of mouse keys.
	enum class MouseState {
		DOWN,
		PRESSED,
		UP,
		RELEASED
	};

	// Updates previous key states for key up and down check.
	void UpdateKeyStates(const std::uint8_t* key_states);

	// Updates previous mouse states for mouse up and down check.
	void UpdateMouseState(Mouse button);

	/*
	* @param button Mouse enum corresponding to the desired button.
	* @return Pair of pointers to the mouse state and timer for a given button,
	* pair of nullptrs if no such button exists.
	*/
	std::pair<MouseState&, Timer&> GetMouseStateAndTimer(Mouse button);

	/*
	* @param button Mouse enum corresponding to the desired button.
	* @return Current state of the given mouse button.
	*/
	MouseState GetMouseState(Mouse button) const;

	milliseconds GetMouseHeldTime(Mouse button);

	void Update();

	V2_int GetMousePosition();

	bool MousePressed(Mouse button) const;

	bool MouseReleased(Mouse button) const;

	bool MouseDown(Mouse button) const;

	bool MouseUp(Mouse button) const;

	bool KeyPressed(Key key) const;

	bool KeyReleased(Key key) const;

	bool KeyDown(Key key) const;

	bool KeyUp(Key key) const;
private:
	// Number of keys stored in the SDL key states array. For creating previous key states array.
	static constexpr std::size_t KEY_COUNT{ 512 };

	// Previous loop cycle key states for comparison with current.
	std::array<std::uint8_t, KEY_COUNT> previous_key_states_{};

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