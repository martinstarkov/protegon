#pragma once

#include <iostream>
#include <array>
#include <cstdlib>

#include "math/Vector2.h"
#include "event/Keys.h"

enum class MouseButton {
	LEFT,
	RIGHT,
	MIDDLE
};

namespace engine {

constexpr std::size_t KEY_COUNT = 512;

class InputHandler {
public:
	static void Update();
	static V2_int GetMousePosition();
	static bool MousePressed(MouseButton button);
	static bool MouseHeld(MouseButton button);
	static bool MouseReleased(MouseButton button);
	static bool MouseHeldFor(MouseButton button, std::uint64_t cycles);

	static bool KeyPressed(Key key);
	static bool KeyReleased(Key key);
	static bool KeyDown(Key key);
	static bool KeyUp(Key key);
private:
	enum class MouseState {
		RELEASED,
		PRESSED,
		HELD
	};
	enum class KeyState {
		RELEASED = 0,
		PRESSED = 1
	};
	static MouseState GetMouseState(MouseButton button);
	static std::uint64_t GetMouseHoldCycles(MouseButton button);
	static void UpdateMouse();
	static void UpdateKeyboard();
	static void MouseButtonReleased(MouseState& mouse_button_state, std::uint64_t& mouse_button_pressed_time);
	static void MouseButtonPressed(MouseState& mouse_button_state, std::uint64_t& mouse_button_pressed_time);
	// Pressed time (in cycles) used for mouse hold check
	static std::uint64_t left_mouse_pressed_time_;
	static std::uint64_t right_mouse_pressed_time_;
	static std::uint64_t middle_mouse_pressed_time_;
	static MouseState left_mouse_;
	static MouseState right_mouse_;
	static MouseState middle_mouse_;
	static V2_int mouse_position_;
	static std::array<std::uint8_t, KEY_COUNT> key_states_;
	static std::array<std::uint8_t, KEY_COUNT> previous_key_states_;
	friend std::ostream& operator<<(std::ostream& os, const MouseState& state);
};

} // namespace engine