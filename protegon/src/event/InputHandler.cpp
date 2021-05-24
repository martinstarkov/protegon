#include "InputHandler.h"

#include <SDL.h>

#include <algorithm> // std::copy

#include "core/Engine.h"
#include "debugging/Debug.h"

namespace engine {

void InputHandler::Update() {
	auto& instance{ GetInstance() };
	// Update previous key states.
	instance.UpdateKeyStates();
	// Update mouse states.
	instance.UpdateMouseState(Mouse::LEFT);
	instance.UpdateMouseState(Mouse::RIGHT);
	instance.UpdateMouseState(Mouse::MIDDLE);
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_MOUSEBUTTONDOWN: {
				auto [state, timer] = instance.GetMouseStateAndTimer(static_cast<Mouse>(event.button.button));
				timer->Start();
				*state = MouseState::DOWN;
				break;
			}
			case SDL_MOUSEBUTTONUP: {
				auto [state, timer] = instance.GetMouseStateAndTimer(static_cast<Mouse>(event.button.button));
				timer->Reset();
				*state = MouseState::UP;
				break;
			}
			case SDL_QUIT: {
				Engine::Quit();
				break;
			}
			case SDL_WINDOWEVENT: {
				// Possible window events here in the future.
				/*switch (event.window.event) {
					default:
						break;
				}*/
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

void InputHandler::UpdateMouseState(Mouse button) {
	auto [state, timer] = GetMouseStateAndTimer(button);
	if (timer->IsRunning() && *state == MouseState::DOWN) {
		*state = MouseState::PRESSED;
	} else if (!timer->IsRunning() && *state == MouseState::UP) {
		*state = MouseState::RELEASED;
	}
}

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

V2_int InputHandler::GetMousePosition() {
	auto& instance{ GetInstance() };
	// Grab latest mouse events from queue.
	SDL_PumpEvents();
	// Update mouse position.
	SDL_GetMouseState(&instance.mouse_position_.x, &instance.mouse_position_.y);
	return instance.mouse_position_;
}

InputHandler::MouseState InputHandler::GetMouseState(Mouse button) {
	auto& instance{ GetInstance() };
	auto [state, timer] = instance.GetMouseStateAndTimer(button);
	return *state;
}

bool InputHandler::MousePressed(Mouse button) {
	auto state{ GetMouseState(button) };
	return state == MouseState::PRESSED || state == MouseState::DOWN;
}

bool InputHandler::MouseReleased(Mouse button) {
	auto state{ GetMouseState(button) };
	return state == MouseState::RELEASED || state == MouseState::UP;
}

bool InputHandler::MouseDown(Mouse button) {
	return GetMouseState(button) == MouseState::DOWN;
}

bool InputHandler::MouseUp(Mouse button) {
	return GetMouseState(button) == MouseState::UP;
}

bool InputHandler::KeyPressed(Key key) {
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