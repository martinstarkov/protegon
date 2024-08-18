#include "input_handler.h"

#include <algorithm>

#include "SDL.h"
#include "protegon/game.h"
#include "protegon/log.h"
#include "utility/debug.h"

namespace ptgn {

void InputHandler::Reset() {
	key_states_.reset();
	first_time_down_.reset();
	first_time_up_.reset();

	// Mouse states.
	left_mouse_		= MouseState::Released;
	right_mouse_	= MouseState::Released;
	middle_mouse_	= MouseState::Released;
	mouse_position_ = {};
	mouse_scroll_	= {};

	// Mouse button held for timers.

	left_mouse_timer_	= {};
	right_mouse_timer_	= {};
	middle_mouse_timer_ = {};
}

void InputHandler::Update() {
	// Update mouse states.
	UpdateMouseState(Mouse::Left);
	UpdateMouseState(Mouse::Right);
	UpdateMouseState(Mouse::Middle);
	mouse_scroll_ = {};
	SDL_Event e;
	while (SDL_PollEvent(&e)) {
		switch (e.type) {
			case SDL_MOUSEMOTION: {
				V2_int previous{ mouse_position_ };
				SDL_MouseMotionEvent* m = (SDL_MouseMotionEvent*)&e;
				mouse_position_.x		= m->x;
				mouse_position_.y		= m->y;
				game.event.mouse.Post(
					MouseEvent::Move, MouseMoveEvent{ previous, mouse_position_ }
				);
				break;
			}
			case SDL_MOUSEBUTTONDOWN: {
				std::pair<MouseState&, Timer&> pair =
					GetMouseStateAndTimer(static_cast<Mouse>(e.button.button));
				pair.second.Start();
				pair.first = MouseState::Down;
				game.event.mouse.Post(
					MouseEvent::Down,
					MouseDownEvent{ static_cast<Mouse>(e.button.button), mouse_position_ }
				);
				break;
			}
			case SDL_MOUSEBUTTONUP: {
				std::pair<MouseState&, Timer&> pair =
					GetMouseStateAndTimer(static_cast<Mouse>(e.button.button));
				pair.second.Reset();
				pair.first = MouseState::Up;
				game.event.mouse.Post(
					MouseEvent::Up,
					MouseUpEvent{ static_cast<Mouse>(e.button.button), mouse_position_ }
				);
				break;
			}
			case SDL_MOUSEWHEEL: {
				mouse_scroll_ = { e.wheel.x, e.wheel.y };
				game.event.mouse.Post(MouseEvent::Scroll, MouseScrollEvent{ mouse_scroll_ });
				break;
			}
			case SDL_KEYUP: {
				if (key_states_[e.key.keysym.scancode]) {
					first_time_up_[e.key.keysym.scancode] = true;
				}
				key_states_[e.key.keysym.scancode] = false;
				game.event.key.Post(
					KeyEvent::Up, KeyUpEvent{ static_cast<Key>(e.key.keysym.scancode) }
				);
				break;
			}
			case SDL_KEYDOWN: {
				if (!key_states_[e.key.keysym.scancode]) {
					first_time_down_[e.key.keysym.scancode] = true;
					game.event.key.Post(
						KeyEvent::Down, KeyDownEvent{ static_cast<Key>(e.key.keysym.scancode) }
					);
				}
				key_states_[e.key.keysym.scancode] = true;
				game.event.key.Post(
					KeyEvent::Pressed, KeyPressedEvent{ static_cast<Key>(e.key.keysym.scancode) }
				);
				break;
			}
			case SDL_QUIT: {
				game.event.window.Post(WindowEvent::Quit, WindowQuitEvent{});
				break;
			}
			case SDL_WINDOWEVENT: {
				switch (e.window.event) {
					case SDL_WINDOWEVENT_SIZE_CHANGED:
					case SDL_WINDOWEVENT_RESIZED:	   {
						V2_int window_size{ e.window.data1, e.window.data2 };
						game.event.window.Post(
							WindowEvent::Resized, WindowResizedEvent{ window_size }
						);
						break;
					}
					default: break;
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
	// TODO: Confirm whether or not mouse scaling is required when on different window DPIs.
	// Maybe an SDL hint? Update mouse position.
	SDL_GetMouseState(&mouse_position_.x, &mouse_position_.y);
}

V2_int InputHandler::GetMousePosition() {
	ForceUpdateMousePosition();
	return mouse_position_;
}

int InputHandler::GetMouseScroll() const {
	return mouse_scroll_.y;
}

milliseconds InputHandler::GetMouseHeldTime(Mouse button) {
	std::pair<MouseState&, Timer&> pair = GetMouseStateAndTimer(button);
	// Retrieve held time in nanoseconds for maximum precision.
	const auto held_time{ pair.second.Elapsed<milliseconds>() };
	// Comparison units handled by chrono.
	return held_time;
}

inline int WindowEventWatcher(void* data, SDL_Event* event) {
	if (event->type == SDL_WINDOWEVENT) {
		if (event->window.event == SDL_WINDOWEVENT_RESIZED ||
			event->window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
			SDL_Window* win = SDL_GetWindowFromID(event->window.windowID);
			if (win == (SDL_Window*)data) {
				V2_int window_size{ event->window.data1, event->window.data2 };
				game.event.window.Post(WindowEvent::Resizing, WindowResizingEvent{ window_size });
			}
		} else if (event->window.event == SDL_WINDOWEVENT_EXPOSED) {
			game.event.window.Post(WindowEvent::Drag, WindowDragEvent{});
		}
	}
	return 0;
}

InputHandler::InputHandler() {
	SDL_AddEventWatch(WindowEventWatcher, game.window.GetSDLWindow());
}

InputHandler::~InputHandler() {
	SDL_DelEventWatch(WindowEventWatcher, game.window.GetSDLWindow());
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
	PTGN_ERROR("Input handler cannot retrieve state and timer for invalid mouse button");
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
	PTGN_ASSERT(
		key_number < InputHandler::key_count_, "Could not find key in input handler key states"
	);
	return key_states_[key_number];
}

bool InputHandler::KeyReleased(Key key) const {
	return !KeyPressed(key);
}

bool InputHandler::KeyDown(Key key) {
	auto key_number{ static_cast<std::size_t>(key) };
	PTGN_ASSERT(
		key_number < InputHandler::key_count_, "Could not find key in input handler key states"
	);
	if (first_time_down_[key_number]) {
		first_time_up_[key_number]	 = false;
		first_time_down_[key_number] = false;
		return true;
	}
	return false;
}

bool InputHandler::KeyUp(Key key) {
	auto key_number{ static_cast<std::size_t>(key) };
	PTGN_ASSERT(
		key_number < InputHandler::key_count_, "Could not find key in input handler key states"
	);
	if (first_time_up_[key_number]) {
		first_time_up_[key_number]	 = false;
		first_time_down_[key_number] = false;
		return true;
	}
	return false;
}

} // namespace ptgn
