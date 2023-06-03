#include "input_handler.h"

#include <cassert>   // assert
#include <algorithm> // std::copy

#include <SDL.h>

#include "core/game.h"
#include <protegon/log.h>

namespace ptgn {

void InputHandler::Update() {
	// Update mouse states.
	UpdateMouseState(Mouse::LEFT);
	UpdateMouseState(Mouse::RIGHT);
	UpdateMouseState(Mouse::MIDDLE);
	first_time_.reset();
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_MOUSEBUTTONDOWN:
			{
				auto& [state, timer] = GetMouseStateAndTimer(static_cast<Mouse>(event.button.button));
				timer.Start();
				state = InputHandler::MouseState::DOWN;
				break;
			}
			case SDL_MOUSEBUTTONUP:
			{
				auto& [state, timer] = GetMouseStateAndTimer(static_cast<Mouse>(event.button.button));
				timer.Reset();
				state = InputHandler::MouseState::UP;
				break;
			}
			case SDL_KEYUP: {
				if (key_states_[event.key.keysym.scancode]) {
					first_time_[event.key.keysym.scancode] = true;
				}
				key_states_[event.key.keysym.scancode] = false;
				break;
			}
			case SDL_KEYDOWN: {
				if (!key_states_[event.key.keysym.scancode]) {
					first_time_[event.key.keysym.scancode] = true;
				}
				key_states_[event.key.keysym.scancode] = true;
				break;
			}
			case SDL_QUIT:
			{
				// TODO: Ensure this doesn't crash anything.
				SDL_DestroyWindow(global::GetGame().sdl.GetWindow());
				break;
			}
			// Possible window events here in the future.
			/*
			case SDL_WINDOWEVENT: {
				switch (event.window.event) {
					default:
						break;
				}
				break;
			}
			*/
			default:
				break;
		}
	}
}

V2_int InputHandler::GetMousePosition() {
	// Grab latest mouse events from queue.
	SDL_PumpEvents();
	// Update mouse position.
	V2_int mouse_position;
	SDL_GetMouseState(&mouse_position.x, &mouse_position.y);
	return mouse_position;
}

milliseconds InputHandler::GetMouseHeldTime(Mouse button) {
	auto& [state, timer] = GetMouseStateAndTimer(button);
	// Retrieve held time in nanoseconds for maximum precision.
	const auto held_time{ timer.Elapsed<milliseconds>() };
	// Comparison units handled by chrono.
	return held_time;
}

void InputHandler::UpdateMouseState(Mouse button) {
	auto& [state, timer] = GetMouseStateAndTimer(button);
	if (timer.IsRunning() && state == MouseState::DOWN) {
		state = MouseState::PRESSED;
	} else if (!timer.IsRunning() && state == MouseState::UP) {
		state = MouseState::RELEASED;
	}
}

std::pair<InputHandler::MouseState&, Timer&> InputHandler::GetMouseStateAndTimer(Mouse button) {
	switch (button) {
		case Mouse::LEFT:
			return { left_mouse_, left_mouse_timer_ };
		case Mouse::RIGHT:
			return { right_mouse_, right_mouse_timer_ };
		case Mouse::MIDDLE:
			return { middle_mouse_, middle_mouse_timer_ };
		default:
			break;
	}
	assert(!"Input handler cannot retrieve state and timer for invalid mouse button");
	return { middle_mouse_, middle_mouse_timer_ }; // unused but avoids control path error.
}

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

bool InputHandler::MousePressed(Mouse button) const {
	auto state{ GetMouseState(button) };
	return state == InputHandler::MouseState::PRESSED || state == InputHandler::MouseState::DOWN;
}

bool InputHandler::MouseReleased(Mouse button) const {
	auto state{ GetMouseState(button) };
	return state == InputHandler::MouseState::RELEASED || state == InputHandler::MouseState::UP;
}

bool InputHandler::MouseDown(Mouse button) const {
	return GetMouseState(button) == InputHandler::MouseState::DOWN;
}

bool InputHandler::MouseUp(Mouse button) const {
	return GetMouseState(button) == InputHandler::MouseState::UP;
}

bool InputHandler::KeyPressed(Key key) const {
	auto key_number{ static_cast<std::size_t>(key) };
	assert(key_number < InputHandler::KEY_COUNT && "Could not find key in input handler key states");
	return key_states_[key_number];
}

bool InputHandler::KeyReleased(Key key) const {
	return !KeyPressed(key);
}

bool InputHandler::KeyDown(Key key) {
	auto key_number{ static_cast<std::size_t>(key) };
	assert(key_number < InputHandler::KEY_COUNT && "Could not find key in input handler key states");
	return first_time_[key_number] && key_states_[key_number];
}

bool InputHandler::KeyUp(Key key) {
	auto key_number{ static_cast<std::size_t>(key) };
	assert(key_number < InputHandler::KEY_COUNT && "Could not find key in input handler key states");
	return first_time_[key_number] && !key_states_[key_number];
}

} // namespace ptgn