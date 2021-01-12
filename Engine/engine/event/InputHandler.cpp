#include "InputHandler.h"

#include <cassert>
#include <algorithm>

#include <SDL.h>

#include "core/Engine.h"

namespace engine {

constexpr double MOUSE_HOLD_TIME = 0.25; // seconds

std::array<std::uint8_t, KEYS> InputHandler::key_states_;
std::array<std::uint8_t, KEYS> InputHandler::previous_key_states_;
InputHandler::MouseState InputHandler::left_mouse_{ MouseState::RELEASED };
InputHandler::MouseState InputHandler::right_mouse_{ MouseState::RELEASED };
InputHandler::MouseState InputHandler::middle_mouse_{ MouseState::RELEASED };
std::uint64_t InputHandler::left_mouse_pressed_time_{ 0 };
std::uint64_t InputHandler::right_mouse_pressed_time_{ 0 };
std::uint64_t InputHandler::middle_mouse_pressed_time_{ 0 };
V2_int InputHandler::mouse_position_{ 0, 0 };

void InputHandler::Init() {
	const std::uint8_t* state = SDL_GetKeyboardState(NULL);
	std::copy(state, state + KEYS, std::begin(key_states_));
	std::copy(std::begin(key_states_), std::end(key_states_), std::begin(previous_key_states_));
	LOG("Initialized input handler");
}

void InputHandler::Update() {
	SDL_Event e;
	while (SDL_PollEvent(&e)) {
		switch (e.type) {
			case SDL_QUIT:
				Engine::Quit();
				break;
			default:
				break;
		}
	}
	UpdateMouse();
	UpdateKeyboard();
}

void InputHandler::UpdateKeyboard() {
	std::copy(std::begin(key_states_), std::end(key_states_), std::begin(previous_key_states_));
	const std::uint8_t* state = SDL_GetKeyboardState(NULL);
	std::copy(state, state + KEYS, std::begin(key_states_));
}

bool InputHandler::KeyPressed(Key key) {
	assert(key_states_.size() > 0 && "Could not find keyboard state");
	return key_states_[static_cast<SDL_Scancode>(key)];
}

bool InputHandler::KeyReleased(Key key) {
	assert(key_states_.size() > 0 && "Could not find keyboard state");
	return !key_states_[static_cast<SDL_Scancode>(key)];
}

bool InputHandler::KeyDown(Key key) {
	return KeyPressed(key) && !previous_key_states_[static_cast<SDL_Scancode>(key)];
}

bool InputHandler::KeyUp(Key key) {
	return KeyReleased(key) && previous_key_states_[static_cast<SDL_Scancode>(key)];
}

void InputHandler::UpdateMouse() {
	auto flags = SDL_GetMouseState(&mouse_position_.x, &mouse_position_.y);
	if (flags) {
		if (flags & SDL_BUTTON_LMASK) {
			MouseButtonPressed(left_mouse_, left_mouse_pressed_time_);
		} else {
			MouseButtonReleased(left_mouse_, left_mouse_pressed_time_);
		}
		if (flags & SDL_BUTTON_RMASK) {
			MouseButtonPressed(right_mouse_, right_mouse_pressed_time_);
		} else {
			MouseButtonReleased(right_mouse_, right_mouse_pressed_time_);
		}
		if (flags & SDL_BUTTON_MMASK) {
			MouseButtonPressed(middle_mouse_, middle_mouse_pressed_time_);
		} else {
			MouseButtonReleased(middle_mouse_, middle_mouse_pressed_time_);
		}
	} else {
		MouseButtonReleased(left_mouse_, left_mouse_pressed_time_);
		MouseButtonReleased(right_mouse_, right_mouse_pressed_time_);
		MouseButtonReleased(middle_mouse_, middle_mouse_pressed_time_);
	}
}

V2_int InputHandler::GetMousePosition() {
	SDL_GetMouseState(&mouse_position_.x, &mouse_position_.y);
	return mouse_position_;
}

InputHandler::MouseState InputHandler::GetMouseState(MouseButton button) {
	switch (button) {
		case MouseButton::LEFT:
			return left_mouse_;
		case MouseButton::RIGHT:
			return right_mouse_;
		case MouseButton::MIDDLE:
			return middle_mouse_;
		default:
			return MouseState::RELEASED;
	}
}

bool InputHandler::MousePressed(MouseButton button) {
	return GetMouseState(button) == MouseState::PRESSED || GetMouseState(button) == MouseState::HELD;
}

bool InputHandler::MouseHeld(MouseButton button) {
	return GetMouseState(button) == MouseState::HELD;
}

bool InputHandler::MouseReleased(MouseButton button) {
	return GetMouseState(button) == MouseState::RELEASED;
}

std::uint64_t InputHandler::GetMouseHoldCycles(MouseButton button) {
	switch (button) {
		case MouseButton::LEFT:
			return left_mouse_pressed_time_;
		case MouseButton::RIGHT:
			return right_mouse_pressed_time_;
		case MouseButton::MIDDLE:
			return middle_mouse_pressed_time_;
		default:
			return 0;
	}
}

bool InputHandler::MouseHeldFor(MouseButton button, std::uint64_t cycles) {
	return GetMouseHoldCycles(button) > cycles;
}

void InputHandler::MouseButtonReleased(MouseState& mouse_button_state, std::uint64_t& mouse_button_pressed_time) {
	mouse_button_state = MouseState::RELEASED;
	mouse_button_pressed_time = 0;
}

void InputHandler::MouseButtonPressed(MouseState& mouse_button_state, std::uint64_t& mouse_button_pressed_time) {
	mouse_button_state = MouseState::PRESSED;
	// How many frames the mouse has been pressed down for.
	if (mouse_button_pressed_time > static_cast<std::uint64_t>(MOUSE_HOLD_TIME * static_cast<double>(Engine::FPS()))) {
		mouse_button_state = MouseState::HELD;
	}
	++mouse_button_pressed_time;
}

inline std::ostream& operator<<(std::ostream& os, const Key& key) {
	os << SDL_GetKeyName(SDL_GetKeyFromScancode(static_cast<SDL_Scancode>(key)));
	return os;
}

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

std::ostream& operator<<(std::ostream& os, const InputHandler::MouseState& state) {
	switch (state) {
		case InputHandler::MouseState::RELEASED:
			os << "released";
			return os;
		case InputHandler::MouseState::PRESSED:
			os << "pressed";
			return os;
		case InputHandler::MouseState::HELD:
			os << "held";
			return os;
		default:
			return os;
	}
	return os;
}

} // namespace engine