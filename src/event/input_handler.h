#pragma once

#include <bitset>
#include <cstdint>
#include <cstdlib>
#include <tuple>

#include "protegon/key.h"
#include "protegon/mouse.h"
#include "protegon/timer.h"
#include "protegon/vector2.h"

namespace ptgn {

struct InputHandler {
	// Updates previous mouse states for mouse up and down check.
	void UpdateMouseState(Mouse button);

	/*
	 * @param button Mouse enum corresponding to the desired button.
	 * @return Pair of pointers to the mouse state and timer for a given button,
	 * pair of nullptrs if no such button exists.
	 */
	[[nodiscard]] std::pair<MouseState&, Timer&> GetMouseStateAndTimer(
		Mouse button
	);

	/*
	 * @param button Mouse enum corresponding to the desired button.
	 * @return Current state of the given mouse button.
	 */
	[[nodiscard]] MouseState GetMouseState(Mouse button) const;

	[[nodiscard]] milliseconds GetMouseHeldTime(Mouse button);

	void Update();

	void ForceUpdateMousePosition();
	[[nodiscard]] V2_int GetMousePosition();

	// @return The amount scrolled by the mouse vertically in the current frame,
	// positive upward, negative downward.
	[[nodiscard]] int GetMouseScroll() const;

	[[nodiscard]] bool MousePressed(Mouse button) const;

	[[nodiscard]] bool MouseReleased(Mouse button) const;

	[[nodiscard]] bool MouseDown(Mouse button) const;

	[[nodiscard]] bool MouseUp(Mouse button) const;

	[[nodiscard]] bool KeyPressed(Key key) const;

	[[nodiscard]] bool KeyReleased(Key key) const;

	[[nodiscard]] bool KeyDown(Key key);

	[[nodiscard]] bool KeyUp(Key key);

private:
	// Number of keys stored in the SDL key states array. For creating previous
	// key states array.
	static constexpr std::size_t KEY_COUNT{ 512 };

	// Previous loop cycle key states for comparison with current.
	std::bitset<KEY_COUNT> key_states_{};
	std::bitset<KEY_COUNT> first_time_{};

	// Mouse states.
	MouseState left_mouse_{ MouseState::Released };
	MouseState right_mouse_{ MouseState::Released };
	MouseState middle_mouse_{ MouseState::Released };
	V2_int mouse_position;
	V2_int mouse_scroll;

	// Mouse button held for timers.

	Timer left_mouse_timer_;
	Timer right_mouse_timer_;
	Timer middle_mouse_timer_;
};

} // namespace ptgn