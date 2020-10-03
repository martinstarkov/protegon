#include "InputHandler.h"

#include <cassert>

#include <engine/core/Engine.h>

namespace engine {

// Unit: Seconds
#define MOUSE_HOLD_TIME 0.25
#define MOUSE_HOLD_CYCLES static_cast<std::uint64_t>(MOUSE_HOLD_TIME * Engine::FPS())

InputHandler::MouseState InputHandler::left_mouse{ MouseState::RELEASED };
InputHandler::MouseState InputHandler::right_mouse{ MouseState::RELEASED };
InputHandler::MouseState InputHandler::middle_mouse{ MouseState::RELEASED };
std::uint64_t InputHandler::left_mouse_pressed_time{ 0 };
std::uint64_t InputHandler::right_mouse_pressed_time{ 0 };
std::uint64_t InputHandler::middle_mouse_pressed_time{ 0 };
V2_int InputHandler::mouse_position{};

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
}

void InputHandler::UpdateMouse() {
	auto flags = SDL_GetMouseState(&mouse_position.x, &mouse_position.y);
	if (flags) {
		if (flags & SDL_BUTTON_LMASK) {
			MouseButtonPressed(left_mouse, left_mouse_pressed_time);
		} else {
			MouseButtonReleased(left_mouse, left_mouse_pressed_time);
		}
		if (flags & SDL_BUTTON_RMASK) {
			MouseButtonPressed(right_mouse, right_mouse_pressed_time);
		} else {
			MouseButtonReleased(right_mouse, right_mouse_pressed_time);
		}
		if (flags & SDL_BUTTON_MMASK) {
			MouseButtonPressed(middle_mouse, middle_mouse_pressed_time);
		} else {
			MouseButtonReleased(middle_mouse, middle_mouse_pressed_time);
		}
	} else {
		MouseButtonReleased(left_mouse, left_mouse_pressed_time);
		MouseButtonReleased(right_mouse, right_mouse_pressed_time);
		MouseButtonReleased(middle_mouse, middle_mouse_pressed_time);
	}
}

V2_int InputHandler::GetMousePosition() {
	SDL_GetMouseState(&mouse_position.x, &mouse_position.y);
	return mouse_position;
}

InputHandler::MouseState InputHandler::GetMouseState(MouseButton button) {
	switch (button) {
		case MouseButton::LEFT:
			return left_mouse;
		case MouseButton::RIGHT:
			return right_mouse;
		case MouseButton::MIDDLE:
			return middle_mouse;
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
			return left_mouse_pressed_time;
		case MouseButton::RIGHT:
			return right_mouse_pressed_time;
		case MouseButton::MIDDLE:
			return middle_mouse_pressed_time;
		default:
			return 0;
	}
}

bool InputHandler::HeldFor(MouseButton button, std::uint64_t cycles) {
	return GetMouseHoldCycles(button) > cycles;
}

void InputHandler::MouseButtonReleased(MouseState& mouse_button_state, std::uint64_t& mouse_button_pressed_time) {
	mouse_button_state = MouseState::RELEASED;
	mouse_button_pressed_time = 0;
}

void InputHandler::MouseButtonPressed(MouseState& mouse_button_state, std::uint64_t& mouse_button_pressed_time) {
	mouse_button_state = MouseState::PRESSED;
	if (mouse_button_pressed_time > MOUSE_HOLD_CYCLES) {
		mouse_button_state = MouseState::HELD;
	}
	++mouse_button_pressed_time;
}

} // namespace engine