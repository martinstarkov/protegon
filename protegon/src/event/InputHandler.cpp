#include "InputHandler.h"

#include <cassert>
#include <algorithm>

#include <SDL.h>

#include "core/Engine.h"

namespace engine {

InputHandler* InputHandler::instance_{ nullptr };

void InputHandler::Init() {
	assert(instance_ == nullptr && "Cannot initialize input handler more than once");
	instance_ = new InputHandler{};
}

InputHandler& InputHandler::GetInstance() {
	assert(instance_ != nullptr && "Input handler has not been initialized properly");
	return *instance_;
}

void InputHandler::Update() {
	auto& instance{ GetInstance() };
	instance.UpdateKeyStates();
	instance.UpdatePreviousMouseStates();
	instance.UpdateMouseStates();
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_QUIT:
				Engine::Quit();
				break;
			case SDL_WINDOWEVENT:
			{
				switch (event.window.event) {
					case SDL_WINDOWEVENT_CLOSE:
						Engine::Quit(SDL_GetWindowFromID(event.window.windowID));
						break;
					default:
						break;
				}
				break;
			}
			default:
				break;
		}
	}
}

void InputHandler::UpdateKeyStates() {
	const auto key_states{ SDL_GetKeyboardState(NULL) };
	// Copy current key states to previous key states.
	std::copy(key_states, key_states + KEY_COUNT, std::begin(previous_key_states_));
}

void InputHandler::UpdatePreviousMouseStates() {
	left_was_pressed_ = left_mouse_pressed_time_.IsRunning();
	right_was_pressed_ = right_mouse_pressed_time_.IsRunning();
	middle_was_pressed_ = middle_mouse_pressed_time_.IsRunning();
}

void InputHandler::UpdateMouseStates() {
	SDL_PumpEvents();
	auto flags{ SDL_GetMouseState(&mouse_position_.x, &mouse_position_.y) };
	if (flags) {
		if (flags & SDL_BUTTON(SDL_BUTTON_LEFT)) {
			if (!left_mouse_pressed_time_.IsRunning()) {
				left_mouse_pressed_time_.Start();
			}
		} else {
			left_mouse_pressed_time_.Reset();
		}
		if (flags & SDL_BUTTON(SDL_BUTTON_RIGHT)) {
			if (!right_mouse_pressed_time_.IsRunning()) {
				right_mouse_pressed_time_.Start();
			}
		} else {
			right_mouse_pressed_time_.Reset();
		}
		if (flags & SDL_BUTTON(SDL_BUTTON_MIDDLE)) {
			if (!middle_mouse_pressed_time_.IsRunning()) {
				middle_mouse_pressed_time_.Start();
			}
		} else {
			middle_mouse_pressed_time_.Reset();
		}
	} else {
		left_mouse_pressed_time_.Reset();
		right_mouse_pressed_time_.Reset();
		middle_mouse_pressed_time_.Reset();
	}
}

V2_int InputHandler::GetMousePosition() {
	auto& instance{ GetInstance() };
	return instance.mouse_position_;
}

std::int64_t InputHandler::GetMousePressedTime(MouseButton button) {
	switch (button) {
		case MouseButton::LEFT:
			return left_mouse_pressed_time_.ElapsedMilliseconds();
		case MouseButton::RIGHT:
			return right_mouse_pressed_time_.ElapsedMilliseconds();
		case MouseButton::MIDDLE:
			return middle_mouse_pressed_time_.ElapsedMilliseconds();
		default:
			return 0;
	}
	return 0;
}

bool InputHandler::GetPreviousMouseState(MouseButton button) {
	switch (button) {
		case MouseButton::LEFT:
			return left_was_pressed_;
		case MouseButton::RIGHT:
			return right_was_pressed_;
		case MouseButton::MIDDLE:
			return middle_was_pressed_;
		default:
			return false;
	}
	return false;
}

bool InputHandler::MouseHeldFor(MouseButton button, std::int64_t milliseconds) {
	auto& instance{ GetInstance() };
	auto pressed_time{ instance.GetMousePressedTime(button) };
	return pressed_time > milliseconds;
}

bool InputHandler::MousePressed(MouseButton button) {
	return MouseHeldFor(button, 0);
}

bool InputHandler::MouseReleased(MouseButton button) {
	return !MousePressed(button);
}

bool InputHandler::MouseDown(MouseButton button) {
	// TODO
	return false;
}

bool InputHandler::MouseUp(MouseButton button) {
	// TODO
	return false;
}

bool InputHandler::KeyPressed(Key key) {
	SDL_PumpEvents();
	const auto key_states{ SDL_GetKeyboardState(NULL) };
	auto key_number{ static_cast<std::size_t>(key) };
	assert(key_number < KEY_COUNT && "Could not find key in input handler key states");
	return key_states[key_number];
}

bool InputHandler::KeyReleased(Key key) {
	return !KeyPressed(key);
}

bool InputHandler::KeyDown(Key key) {
	auto& instance{ GetInstance() };
	return KeyPressed(key) && !instance.previous_key_states_[static_cast<std::size_t>(key)];
}

bool InputHandler::KeyUp(Key key) {
	auto& instance{ GetInstance() };
	return KeyReleased(key) && instance.previous_key_states_[static_cast<std::size_t>(key)];
}

} // namespace engine