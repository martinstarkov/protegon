#pragma once

#include <cstdint>
#include <iostream>
#include <type_traits> 

#include <SDL.h>

#include <engine/utils/Vector2.h>

namespace engine {

enum class Key : std::underlying_type<SDL_Scancode>::type {
	K_0 = SDL_SCANCODE_0,
	K_1 = SDL_SCANCODE_1,
	K_2 = SDL_SCANCODE_2,
	K_3 = SDL_SCANCODE_3,
	K_4 = SDL_SCANCODE_4,
	K_5 = SDL_SCANCODE_5,
	K_6 = SDL_SCANCODE_6,
	K_7 = SDL_SCANCODE_7,
	K_8 = SDL_SCANCODE_8,
	K_9 = SDL_SCANCODE_9,
	ZERO = SDL_SCANCODE_0,
	ONE = SDL_SCANCODE_1,
	TWO = SDL_SCANCODE_2,
	THREE = SDL_SCANCODE_3,
	FOUR = SDL_SCANCODE_4,
	FIVE = SDL_SCANCODE_5,
	SIX = SDL_SCANCODE_6,
	SEVEN = SDL_SCANCODE_7,
	EIGHT = SDL_SCANCODE_8,
	NINE = SDL_SCANCODE_9,
	A = SDL_SCANCODE_A,
	ALT_GR = SDL_SCANCODE_RALT,
	APOSTROPHE = SDL_SCANCODE_APOSTROPHE,
	B = SDL_SCANCODE_B,
	BACKSPACE = SDL_SCANCODE_BACKSPACE,
	BACK_SLASH = SDL_SCANCODE_BACKSLASH,
	BLANK = SDL_SCANCODE_SPACE,
	C = SDL_SCANCODE_C,
	CAPSLOCK = SDL_SCANCODE_CAPSLOCK,
	COMMA = SDL_SCANCODE_COMMA,
	COMMAND = SDL_SCANCODE_RGUI,
	D = SDL_SCANCODE_D,
	DELETE = SDL_SCANCODE_DELETE,
	DOWN = SDL_SCANCODE_DOWN,
	E = SDL_SCANCODE_E,
	END = SDL_SCANCODE_END,
	ENTER = SDL_SCANCODE_RETURN,
	EQUALS = SDL_SCANCODE_EQUALS,
	ESCAPE = SDL_SCANCODE_ESCAPE,
	F = SDL_SCANCODE_F,
	F1 = SDL_SCANCODE_F1,
	F2 = SDL_SCANCODE_F2,
	F3 = SDL_SCANCODE_F3,
	F4 = SDL_SCANCODE_F4,
	F5 = SDL_SCANCODE_F5,
	F6 = SDL_SCANCODE_F6,
	F7 = SDL_SCANCODE_F7,
	F8 = SDL_SCANCODE_F8,
	F9 = SDL_SCANCODE_F9,
	F10 = SDL_SCANCODE_F10,
	F11 = SDL_SCANCODE_F11,
	F12 = SDL_SCANCODE_F12,
	F13 = SDL_SCANCODE_F13,
	F14 = SDL_SCANCODE_F14,
	F15 = SDL_SCANCODE_F15,
	F16 = SDL_SCANCODE_F16,
	F17 = SDL_SCANCODE_F17,
	F18 = SDL_SCANCODE_F18,
	F19 = SDL_SCANCODE_F19,
	F20 = SDL_SCANCODE_F20,
	F21 = SDL_SCANCODE_F21,
	F22 = SDL_SCANCODE_F22,
	F23 = SDL_SCANCODE_F23,
	F24 = SDL_SCANCODE_F24,
	FORWARD_SLASH = SDL_SCANCODE_SLASH,
	G = SDL_SCANCODE_G,
	GRAVE = SDL_SCANCODE_GRAVE,
	H = SDL_SCANCODE_H,
	HOME = SDL_SCANCODE_HOME,
	I = SDL_SCANCODE_I,
	INSERT = SDL_SCANCODE_INSERT,
	J = SDL_SCANCODE_J,
	K = SDL_SCANCODE_K,
	KP_0 = SDL_SCANCODE_KP_0,
	KP_1 = SDL_SCANCODE_KP_1,
	KP_2 = SDL_SCANCODE_KP_2,
	KP_3 = SDL_SCANCODE_KP_3,
	KP_4 = SDL_SCANCODE_KP_4,
	KP_5 = SDL_SCANCODE_KP_5,
	KP_6 = SDL_SCANCODE_KP_6,
	KP_7 = SDL_SCANCODE_KP_7,
	KP_8 = SDL_SCANCODE_KP_8,
	KP_9 = SDL_SCANCODE_KP_9,
	KP_ZERO = SDL_SCANCODE_KP_0,
	KP_ONE = SDL_SCANCODE_KP_1,
	KP_TWO = SDL_SCANCODE_KP_2,
	KP_THREE = SDL_SCANCODE_KP_3,
	KP_FOUR = SDL_SCANCODE_KP_4,
	KP_FIVE = SDL_SCANCODE_KP_5,
	KP_SIX = SDL_SCANCODE_KP_6,
	KP_SEVEN = SDL_SCANCODE_KP_7,
	KP_EIGHT = SDL_SCANCODE_KP_8,
	KP_NINE = SDL_SCANCODE_KP_9,
	KP_AMPERSAND = SDL_SCANCODE_KP_AMPERSAND,
	KP_PERIOD = SDL_SCANCODE_KP_PERIOD,
	KP_DELETE = SDL_SCANCODE_KP_PERIOD,
	KP_ENTER = SDL_SCANCODE_KP_ENTER,
	KP_PLUS = SDL_SCANCODE_KP_PLUS,
	KP_MINUS = SDL_SCANCODE_KP_MINUS,
	KP_MULTIPLY = SDL_SCANCODE_KP_MULTIPLY,
	KP_STAR = SDL_SCANCODE_KP_MULTIPLY,
	KP_ASTERISK = SDL_SCANCODE_KP_MULTIPLY,
	KP_DIVIDE = SDL_SCANCODE_KP_DIVIDE,
	KP_SLASH = SDL_SCANCODE_KP_DIVIDE,
	KP_FORWARD_SLASH = SDL_SCANCODE_KP_DIVIDE,
	KP_NUMLOCK = SDL_SCANCODE_NUMLOCKCLEAR,
	L = SDL_SCANCODE_L,
	LEFT_ALT = SDL_SCANCODE_LALT,
	LEFT_CTRL = SDL_SCANCODE_LCTRL,
	LEFT = SDL_SCANCODE_LEFT,
	LEFT_BRACKET = SDL_SCANCODE_LEFTBRACKET,
	LEFT_SHIFT = SDL_SCANCODE_LSHIFT,
	M = SDL_SCANCODE_M,
	MINUS = SDL_SCANCODE_MINUS,
	N = SDL_SCANCODE_N,
	NUMLOCK = SDL_SCANCODE_NUMLOCKCLEAR,
	O = SDL_SCANCODE_O,
	P = SDL_SCANCODE_P,
	PAGE_DOWN = SDL_SCANCODE_PAGEDOWN,
	PAGE_UP = SDL_SCANCODE_PAGEUP,
	PERIOD = SDL_SCANCODE_PERIOD,
	PRINT_SCREEN = SDL_SCANCODE_PRINTSCREEN,
	PRTSC = SDL_SCANCODE_PRINTSCREEN,
	Q = SDL_SCANCODE_Q,
	R = SDL_SCANCODE_R,
	RIGHT_ALT = SDL_SCANCODE_RALT,
	RIGHT_CTRL = SDL_SCANCODE_RCTRL,
	RETURN = SDL_SCANCODE_RETURN,
	RETURN2 = SDL_SCANCODE_RETURN2,
	RIGHT = SDL_SCANCODE_RIGHT,
	RIGHT_BRACKET = SDL_SCANCODE_RIGHTBRACKET,
	RIGHT_SHIFT = SDL_SCANCODE_RSHIFT,
	S = SDL_SCANCODE_S,
	SCROLL_LOCK = SDL_SCANCODE_SCROLLLOCK,
	SELECT = SDL_SCANCODE_SELECT,
	SEMICOLON = SDL_SCANCODE_SEMICOLON,
	SEPARATOR = SDL_SCANCODE_SEPARATOR,
	SLASH = SDL_SCANCODE_SLASH,
	SLEEP = SDL_SCANCODE_SLEEP,
	SPACE = SDL_SCANCODE_SPACE,
	SPACE_BAR = SDL_SCANCODE_SPACE,
	SPACEBAR = SDL_SCANCODE_SPACE,
	T = SDL_SCANCODE_T,
	TAB = SDL_SCANCODE_TAB,
	TILDE = SDL_SCANCODE_GRAVE,
	U = SDL_SCANCODE_U,
	UP = SDL_SCANCODE_UP,
	V = SDL_SCANCODE_V,
	W = SDL_SCANCODE_W,
	WINDOWS = SDL_SCANCODE_RGUI,
	X = SDL_SCANCODE_X,
	Y = SDL_SCANCODE_Y,
	Z = SDL_SCANCODE_Z
};

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
	static bool MouseHeldFor(MouseButton button, std::uint64_t cycles);

	static bool KeyPressed(Key key);
	static bool KeyReleased(Key key);
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
	static std::uint64_t left_mouse_pressed_time;
	static std::uint64_t right_mouse_pressed_time;
	static std::uint64_t middle_mouse_pressed_time;
	static MouseState left_mouse;
	static MouseState right_mouse;
	static MouseState middle_mouse;
	static V2_int mouse_position;
	static const std::uint8_t* key_states;
	friend std::ostream& operator<<(std::ostream& os, const MouseState& state) {
		switch (state) {
			case MouseState::RELEASED:
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

inline std::ostream& operator<<(std::ostream& os, const Key& key) {
	os << SDL_GetKeyName(SDL_GetKeyFromScancode(static_cast<SDL_Scancode>(key)));
	return os;
}

} // namespace engine