#pragma once

#include <iostream>

#include "SDL_includes.h"
#include "Vector2.h"

namespace engine {

enum class MouseButton {
	LEFT,
	RIGHT,
	MIDDLE
};

class InputHandler {
public:
	static void Update();
	static V2_int GetMousePosition();
	static bool MousePressed(MouseButton button);
	static bool MouseHeld(MouseButton button);
	static bool MouseReleased(MouseButton button);
	static bool HeldFor(MouseButton button, std::uint64_t cycles);
private:
	enum class MouseState {
		RELEASED,
		PRESSED,
		HELD
	};
	static MouseState GetMouseState(MouseButton button);
	static std::uint64_t GetMouseHoldCycles(MouseButton button);
	static void UpdateMouse();
	static void MouseButtonReleased(MouseState& mouse_button_state, std::uint64_t& mouse_button_pressed_time);
	static void MouseButtonPressed(MouseState& mouse_button_state, std::uint64_t& mouse_button_pressed_time);
	// Pressed time (in cycles) used for mouse hold check
	static std::uint64_t left_mouse_pressed_time;
	static std::uint64_t right_mouse_pressed_time;
	static std::uint64_t middle_mouse_pressed_time;
	static MouseState left_mouse;
	static MouseState right_mouse;
	static MouseState middle_mouse;
	static V2_int mouse_position;
	friend std::ostream& operator<<(std::ostream& os, const MouseState& state) {
		switch (state) {
			case InputHandler::MouseState::RELEASED:
				os << "released";
				return os;
			case MouseState::PRESSED:
				os << "pressed";
				return os;
			case MouseState::HELD:
				os << "held";
				return os;
			default:
				return os;
		}
		return os;
	}
};

inline std::ostream& operator<<(std::ostream& os, const MouseButton& button) {
	switch (button) {
		case MouseButton::LEFT:
			os << "left";
			return os;
		case MouseButton::RIGHT:
			os << "right";
			return os;
		case MouseButton::MIDDLE:
			os << "middle";
			return os;
		default:
			return os;
	}
	return os;
}

} // namespace engine