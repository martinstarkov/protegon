#include "input_handler.h"

#include <algorithm>

#include "SDL.h"

#include "protegon/game.h"
#include "protegon/log.h"
#include "utility/debug.h"

namespace ptgn {

void InputHandler::Update() {
	// Update mouse states.
	UpdateMouseState(Mouse::Left);
	UpdateMouseState(Mouse::Right);
	UpdateMouseState(Mouse::Middle);
	mouse_scroll = {};
	first_time_.reset();
	SDL_Event event;
	EventHandler& event_handler{ game.event };
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_MOUSEMOTION: {
				V2_int previous{ mouse_position };
				mouse_position.x = event.button.x;
				mouse_position.y = event.button.y;
				event_handler.mouse.Post(MouseEvent::Move, MouseMoveEvent{ previous, mouse_position });
				break;
			}
			case SDL_MOUSEBUTTONDOWN: {
				std::pair<MouseState&, Timer&> pair =
					GetMouseStateAndTimer(static_cast<Mouse>(event.button.button));
				pair.second.Start();
				pair.first = MouseState::Down;
				event_handler.mouse.Post(MouseEvent::Down, MouseDownEvent{
					static_cast<Mouse>(event.button.button), mouse_position });
				break;
			}
			case SDL_MOUSEBUTTONUP: {
				std::pair<MouseState&, Timer&> pair =
					GetMouseStateAndTimer(static_cast<Mouse>(event.button.button));
				pair.second.Reset();
				pair.first = MouseState::Up;
				event_handler.mouse.Post(MouseEvent::Up, MouseUpEvent{
					static_cast<Mouse>(event.button.button), mouse_position });
				break;
			}
			case SDL_MOUSEWHEEL: {
				mouse_scroll = { event.wheel.x, event.wheel.y };
				event_handler.mouse.Post(
					MouseEvent::Scroll, MouseScrollEvent{ mouse_scroll }
				);
				break;
			}
			case SDL_KEYUP: {
				if (key_states_[event.key.keysym.scancode]) {
					first_time_[event.key.keysym.scancode] = true;
				}
				key_states_[event.key.keysym.scancode] = false;
				// TODO: Add event post here.
				break;
			}
			case SDL_KEYDOWN: {
				if (!key_states_[event.key.keysym.scancode]) {
					first_time_[event.key.keysym.scancode] = true;
				}
				key_states_[event.key.keysym.scancode] = true;
				// TODO: Add event post here.
				break;
			}
			case SDL_QUIT: {
				event_handler.window.Post(WindowEvent::Quit, WindowQuitEvent{});
				break;
			}
			case SDL_WINDOWEVENT: {
				switch (event.window.event) {
					case SDL_WINDOWEVENT_RESIZED: {
						V2_int window_size{ event.window.data1, event.window.data2 };
						event_handler.window.Post(
							WindowEvent::Resize, WindowResizeEvent{ window_size }
						);
						break;
					}
					default:
						break;
				}
				break;
			}
			default: break;
		}
	}
}

void InputHandler::SetRelativeMouseMode(bool on) {
	SDL_SetRelativeMouseMode(static_cast<SDL_bool>(on));
}

void InputHandler::ForceUpdateMousePosition() {
	// Grab latest mouse events from queue.
	SDL_PumpEvents();
	// Update mouse position.
	SDL_GetMouseState(&mouse_position.x, &mouse_position.y);
	// float x, y;
	///*SDL_RenderWindowToLogical(
	//	game.sdl.GetRenderer().get(), mouse_position.x,
	//	mouse_position.y, &x, &y
	//);*/

	// mouse_position.x = static_cast<int>(x);
	// mouse_position.y = static_cast<int>(y);
}

V2_int InputHandler::GetMousePosition() {
	ForceUpdateMousePosition();
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
	if (pair.second.IsRunning() && pair.first == MouseState::Down) {
		pair.first = MouseState::Pressed;
	} else if (!pair.second.IsRunning() && pair.first == MouseState::Up) {
		pair.first = MouseState::Released;
	}
}

std::pair<MouseState&, Timer&> InputHandler::GetMouseStateAndTimer(Mouse button) {
	switch (button) {
		case Mouse::Left:	return { left_mouse_, left_mouse_timer_ };
		case Mouse::Right:	return { right_mouse_, right_mouse_timer_ };
		case Mouse::Middle: return { middle_mouse_, middle_mouse_timer_ };
		default:			break;
	}
	PTGN_ASSERT(false, "Input handler cannot retrieve state and timer for invalid mouse button");
	return { middle_mouse_, middle_mouse_timer_ }; // unused but avoids control path error.
}

MouseState InputHandler::GetMouseState(Mouse button) const {
	switch (button) {
		case Mouse::Left:	return left_mouse_;
		case Mouse::Right:	return right_mouse_;
		case Mouse::Middle: return middle_mouse_;
		default:			return left_mouse_;
	}
}

bool InputHandler::MousePressed(Mouse button) const {
	auto state{ GetMouseState(button) };
	return state == MouseState::Pressed || state == MouseState::Down;
}

bool InputHandler::MouseReleased(Mouse button) const {
	auto state{ GetMouseState(button) };
	return state == MouseState::Released || state == MouseState::Up;
}

bool InputHandler::MouseDown(Mouse button) const {
	return GetMouseState(button) == MouseState::Down;
}

bool InputHandler::MouseUp(Mouse button) const {
	return GetMouseState(button) == MouseState::Up;
}

bool InputHandler::KeyPressed(Key key) const {
	auto key_number{ static_cast<std::size_t>(key) };
	PTGN_CHECK(
		key_number < InputHandler::KEY_COUNT, "Could not find key in input handler key states"
	);
	return key_states_[key_number];
}

bool InputHandler::KeyReleased(Key key) const {
	return !KeyPressed(key);
}

bool InputHandler::KeyDown(Key key) {
	auto key_number{ static_cast<std::size_t>(key) };
	PTGN_CHECK(
		key_number < InputHandler::KEY_COUNT, "Could not find key in input handler key states"
	);
	return first_time_[key_number] && key_states_[key_number];
}

bool InputHandler::KeyUp(Key key) {
	auto key_number{ static_cast<std::size_t>(key) };
	PTGN_CHECK(
		key_number < InputHandler::KEY_COUNT, "Could not find key in input handler key states"
	);
	return first_time_[key_number] && !key_states_[key_number];
}

} // namespace ptgn
