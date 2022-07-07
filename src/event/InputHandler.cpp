#include "InputHandler.h"

#include <cassert> // assert
#include <algorithm> // std::copy

#include <SDL.h>

#include "renderer/Renderer.h"
#include "debugging/Debug.h"
#include "core/Window.h"

// TODO: Fix GetMouseAbsolutePosition function

namespace ptgn {

namespace internal {

void SDLInputHandler::Update() {
	// Update previous key states.
	UpdateKeyStates();
	// Update mouse states.
	UpdateMouseState(Mouse::LEFT);
	UpdateMouseState(Mouse::RIGHT);
	UpdateMouseState(Mouse::MIDDLE);
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_MOUSEBUTTONDOWN: {
				auto [state, timer] = GetMouseStateAndTimer(static_cast<Mouse>(event.button.button));
				timer->Start();
				*state = MouseState::DOWN;
				break;
			}
			case SDL_MOUSEBUTTONUP: {
				auto [state, timer] = GetMouseStateAndTimer(static_cast<Mouse>(event.button.button));
				timer->Reset();
				*state = MouseState::UP;
				break;
			}
			case SDL_QUIT: {
				// TODO: FIX THIS.
				//window::Destroy();
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

void SDLInputHandler::UpdateKeyStates() {
	const auto key_states{ SDL_GetKeyboardState(NULL) };
	// Copy current key states to previous key states.
	std::copy(key_states, key_states + KEY_COUNT, std::begin(previous_key_states_));
}

void SDLInputHandler::UpdateMouseState(Mouse button) {
	auto [state, timer] = GetMouseStateAndTimer(button);
	if (timer->IsRunning() && *state == MouseState::DOWN) {
		*state = MouseState::PRESSED;
	} else if (!timer->IsRunning() && *state == MouseState::UP) {
		*state = MouseState::RELEASED;
	}
}

std::pair<MouseState*, Timer*> SDLInputHandler::GetMouseStateAndTimer(Mouse button) {
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

V2_int SDLInputHandler::GetMouseScreenPosition() const {
	// Grab latest mouse events from queue.
	SDL_PumpEvents();
	// Update mouse position.
	V2_int mouse_position;
	SDL_GetMouseState(&mouse_position.x, &mouse_position.y);
	return mouse_position;
}

V2_int SDLInputHandler::GetMouseAbsolutePosition() const {
	//return WorldRenderer::ScreenToWorld(GetMouseScreenPosition());
	return {};
}

MouseState SDLInputHandler::GetMouseState(Mouse button) const {
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

bool SDLInputHandler::MousePressed(Mouse button) const {
	auto state{ GetMouseState(button) };
	return state == MouseState::PRESSED || state == MouseState::DOWN;
}

bool SDLInputHandler::MouseReleased(Mouse button) const {
	auto state{ GetMouseState(button) };
	return state == MouseState::RELEASED || state == MouseState::UP;
}

bool SDLInputHandler::MouseDown(Mouse button) const {
	return GetMouseState(button) == MouseState::DOWN;
}

bool SDLInputHandler::MouseUp(Mouse button) const {
	return GetMouseState(button) == MouseState::UP;
}

bool SDLInputHandler::KeyPressed(Key key) const {
	const auto key_states{ SDL_GetKeyboardState(NULL) };
	auto key_number{ static_cast<std::size_t>(key) };
	assert(key_number < KEY_COUNT && "Could not find key in input handler key states");
	return key_states[key_number];
}

bool SDLInputHandler::KeyReleased(Key key) const {
	return !KeyPressed(key);
}

bool SDLInputHandler::KeyDown(Key key) const {
	return KeyPressed(key) && !previous_key_states_[static_cast<std::size_t>(key)];
}

bool SDLInputHandler::KeyUp(Key key) const {
	return KeyReleased(key) && previous_key_states_[static_cast<std::size_t>(key)];
}

SDLInputHandler& GetSDLInputHandler() {
	static SDLInputHandler sdl_input_handler;
	return sdl_input_handler;
}

} // namespace internal

namespace services {
	
interfaces::InputHandler& GetInputHandler() {
	return internal::GetSDLInputHandler();
}

} // namespace services

} // namespace ptgn