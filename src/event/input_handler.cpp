#include "input_handler.h"

#include <cassert>   // assert
#include <algorithm> // std::copy

#include <SDL.h>

#include "core/game.h"
#include "protegon/log.h"

namespace ptgn {

void InputHandler::Update() {
	// Update mouse states.
	UpdateMouseState(Mouse::LEFT);
	UpdateMouseState(Mouse::RIGHT);
	UpdateMouseState(Mouse::MIDDLE);
	mouse_scroll = {};
	first_time_.reset();
	SDL_Event event;
	Game& game{ global::GetGame() };
	EventDispatcher<MouseEvent>& mouse_event{ game.event.mouse_event };
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_MOUSEMOTION: {
				V2_int previous{ mouse_position };
				mouse_position.x = event.button.x;
				mouse_position.y = event.button.y;
				mouse_event.Post(MouseMoveEvent{ previous, mouse_position });
				break;
			}
			case SDL_MOUSEBUTTONDOWN:
			{
                std::pair<MouseState&, Timer&> pair = GetMouseStateAndTimer(static_cast<Mouse>(event.button.button));
				pair.second.Start();
				pair.first = MouseState::DOWN;
				mouse_event.Post(MouseDownEvent{ static_cast<Mouse>(event.button.button), mouse_position });
				break;
			}
			case SDL_MOUSEBUTTONUP:
			{
                std::pair<MouseState&, Timer&> pair = GetMouseStateAndTimer(static_cast<Mouse>(event.button.button));
				pair.second.Reset();
				pair.first = MouseState::UP;
				mouse_event.Post(MouseUpEvent{ static_cast<Mouse>(event.button.button), mouse_position });
				break;
			}
			case SDL_MOUSEWHEEL:
			{
				mouse_scroll = { event.wheel.x, event.wheel.y };
				break;
			}
			case SDL_KEYUP: 
			{
				if (key_states_[event.key.keysym.scancode]) {
					first_time_[event.key.keysym.scancode] = true;
				}
				key_states_[event.key.keysym.scancode] = false;
				break;
			}
			case SDL_KEYDOWN: 
			{
				if (!key_states_[event.key.keysym.scancode]) {
					first_time_[event.key.keysym.scancode] = true;
				}
				key_states_[event.key.keysym.scancode] = true;
				break;
			}
			case SDL_QUIT:
			{
				game.Stop();
				return;
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

V2_int InputHandler::GetMousePosition() const {
	// Grab latest mouse events from queue.
	//SDL_PumpEvents();
	// Update mouse position.
	//V2_int mouse_position;
	//SDL_GetMouseState(&mouse_position.x, &mouse_position.y);
	return mouse_position;
}

int InputHandler::GetMouseScroll() const {
	return mouse_scroll.y;
}

milliseconds InputHandler::GetMouseHeldTime(Mouse button) {
    std::pair<MouseState&, Timer&> pair = GetMouseStateAndTimer(button);
	// Retrieve held time in nanoseconds for maximum precision.
	const auto held_time{ pair.second.Elapsed<milliseconds>() };
	// Comparison units handled by chrono.
	return held_time;
}

void InputHandler::UpdateMouseState(Mouse button) {
    std::pair<MouseState&, Timer&> pair = GetMouseStateAndTimer(button);
	if (pair.second.IsRunning() && pair.first == MouseState::DOWN) {
        pair.first = MouseState::PRESSED;
	} else if (!pair.second.IsRunning() && pair.first == MouseState::UP) {
        pair.first = MouseState::RELEASED;
	}
}

std::pair<MouseState&, Timer&> InputHandler::GetMouseStateAndTimer(Mouse button) {
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

MouseState InputHandler::GetMouseState(Mouse button) const {
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
	return state == MouseState::PRESSED || state == MouseState::DOWN;
}

bool InputHandler::MouseReleased(Mouse button) const {
	auto state{ GetMouseState(button) };
	return state == MouseState::RELEASED || state == MouseState::UP;
}

bool InputHandler::MouseDown(Mouse button) const {
	return GetMouseState(button) == MouseState::DOWN;
}

bool InputHandler::MouseUp(Mouse button) const {
	return GetMouseState(button) == MouseState::UP;
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
