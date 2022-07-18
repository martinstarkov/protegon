#include "Input.h"

#include <SDL.h>

#include "interface/Window.h"
#include "input/InputHandler.h"

// TODO: Fix GetMouseAbsolutePosition function

namespace ptgn {

namespace input {

void Update() {
	auto& input_handler{ InputHandler::Get() };
	// Update previous key states.
	const auto key_states{ SDL_GetKeyboardState(NULL) };
	input_handler.UpdateKeyStates(key_states);
	// Update mouse states.
	input_handler.UpdateMouseState(Mouse::LEFT);
	input_handler.UpdateMouseState(Mouse::RIGHT);
	input_handler.UpdateMouseState(Mouse::MIDDLE);
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_MOUSEBUTTONDOWN:
			{
				auto [state, timer] = input_handler.GetMouseStateAndTimer(static_cast<Mouse>(event.button.button));
				timer->Start();
				*state = InputHandler::MouseState::DOWN;
				break;
			}
			case SDL_MOUSEBUTTONUP:
			{
				auto [state, timer] = input_handler.GetMouseStateAndTimer(static_cast<Mouse>(event.button.button));
				timer->Reset();
				*state = InputHandler::MouseState::UP;
				break;
			}
			case SDL_QUIT:
			{
				window::Release();
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

V2_int GetMouseScreenPosition() {
	// Grab latest mouse events from queue.
	SDL_PumpEvents();
	// Update mouse position.
	V2_int mouse_position;
	SDL_GetMouseState(&mouse_position.x, &mouse_position.y);
	return mouse_position;
}

V2_int GetMouseAbsolutePosition() {
	//return WorldRenderer::ScreenToWorld(GetMouseScreenPosition());
	return {};
}

bool MousePressed(Mouse button) {
	auto state{ InputHandler::Get().GetMouseState(button) };
	return state == InputHandler::MouseState::PRESSED || state == InputHandler::MouseState::DOWN;
}

bool MouseReleased(Mouse button) {
	auto state{ InputHandler::Get().GetMouseState(button) };
	return state == InputHandler::MouseState::RELEASED || state == InputHandler::MouseState::UP;
}

bool MouseDown(Mouse button) {
	return InputHandler::Get().GetMouseState(button) == InputHandler::MouseState::DOWN;
}

bool MouseUp(Mouse button) {
	return InputHandler::Get().GetMouseState(button) == InputHandler::MouseState::UP;
}

bool KeyPressed(Key key) {
	const auto key_states{ SDL_GetKeyboardState(NULL) };
	auto key_number{ static_cast<std::size_t>(key) };
	assert(key_number < InputHandler::KEY_COUNT && "Could not find key in input handler key states");
	return key_states[key_number];
}

bool KeyReleased(Key key) {
	return !KeyPressed(key);
}

bool KeyDown(Key key) {
	return KeyPressed(key) && !InputHandler::Get().previous_key_states_[static_cast<std::size_t>(key)];
}

bool KeyUp(Key key) {
	return KeyReleased(key) && InputHandler::Get().previous_key_states_[static_cast<std::size_t>(key)];
}

} // namespace input

} // namespace ptgn