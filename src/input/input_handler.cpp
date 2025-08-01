#include "input/input_handler.h"

#include <bitset>
#include <chrono>
#include <functional>
#include <utility>

#include "common/assert.h"
#include "components/transform.h"
#include "core/game.h"
#include "core/time.h"
#include "core/timer.h"
#include "core/window.h"
#include "debug/log.h"
#include "events/event_handler.h"
#include "events/events.h"
#include "input/key.h"
#include "input/mouse.h"
#include "math/geometry/rect.h"
#include "math/overlap.h"
#include "math/vector2.h"
#include "SDL_events.h"
#include "SDL_keyboard.h"
#include "SDL_mouse.h"
#include "SDL_stdinc.h"
#include "SDL_video.h"

namespace ptgn::impl {

void InputHandler::ResetKeyStates() {
	pressed_keys_.reset();
	previous_pressed_keys_.reset();
}

void InputHandler::ResetMouseStates() {
	left_mouse_			= MouseState::Released;
	right_mouse_		= MouseState::Released;
	middle_mouse_		= MouseState::Released;
	left_mouse_timer_	= {};
	right_mouse_timer_	= {};
	middle_mouse_timer_ = {};
}

void InputHandler::ResetMousePositions() {
	mouse_position_			 = {};
	previous_mouse_position_ = {};
	mouse_scroll_			 = {};
}

void InputHandler::Reset() {
	ResetKeyStates();
	ResetMouseStates();
	ResetMousePositions();
}

void InputHandler::Update() {
	previous_mouse_position_ = mouse_position_;
	previous_pressed_keys_	 = pressed_keys_;
	// Update mouse states.
	UpdateMouseState(Mouse::Left);
	UpdateMouseState(Mouse::Right);
	UpdateMouseState(Mouse::Middle);
	mouse_scroll_ = {};
	SDL_Event e;
	SDL_PumpEvents();
	SDL_GetMouseState(&mouse_position_.x, &mouse_position_.y);
	while (SDL_PollEvent(&e)) {
		switch (e.type) {
			case SDL_MOUSEMOTION: {
				mouse_position_.x = e.motion.x;
				mouse_position_.y = e.motion.y;
				game.event.mouse.Post(MouseEvent::Move, MouseMoveEvent{});
				break;
			}
			case SDL_MOUSEBUTTONDOWN: {
				auto [mouse_state, timer] =
					GetMouseStateAndTimer(static_cast<Mouse>(e.button.button));
				if (mouse_state != MouseState::Pressed) {
					mouse_state = MouseState::Down;
					timer.Start();
					game.event.mouse.Post(
						MouseEvent::Down, MouseDownEvent{ static_cast<Mouse>(e.button.button) }
					);
				}
				break;
			}
			case SDL_MOUSEBUTTONUP: {
				auto [mouse_state, timer] =
					GetMouseStateAndTimer(static_cast<Mouse>(e.button.button));
				timer.Stop();
				mouse_state = MouseState::Up;
				game.event.mouse.Post(
					MouseEvent::Up, MouseUpEvent{ static_cast<Mouse>(e.button.button) }
				);
				break;
			}
			case SDL_KEYDOWN: {
				Key key{ static_cast<Key>(e.key.keysym.scancode) };
				if (!e.key.repeat) {
					pressed_keys_[static_cast<std::size_t>(key)] = true;
					game.event.key.Post(KeyEvent::Down, KeyDownEvent{ key });
				}
				game.event.key.Post(KeyEvent::Pressed, KeyPressedEvent{ key });
				break;
			}
			case SDL_KEYUP: {
				Key key{ static_cast<Key>(e.key.keysym.scancode) };
				if (pressed_keys_[static_cast<std::size_t>(key)]) {
					pressed_keys_[static_cast<std::size_t>(key)] = false;
					game.event.key.Post(KeyEvent::Up, KeyUpEvent{ key });
				}
				break;
			}
			case SDL_MOUSEWHEEL: {
				mouse_scroll_ = { e.wheel.x, e.wheel.y };
				game.event.mouse.Post(MouseEvent::Scroll, MouseScrollEvent{ mouse_scroll_ });
				break;
			}
			case SDL_QUIT: {
				game.Stop();
				game.event.window.Post(WindowEvent::Quit, WindowQuitEvent{});
				break;
			}
			case SDL_WINDOWEVENT: {
				switch (e.window.event) {
					case SDL_WINDOWEVENT_RESIZED:
					case SDL_WINDOWEVENT_SIZE_CHANGED: {
						V2_int window_size{ e.window.data1, e.window.data2 };
						game.event.window.Post(
							WindowEvent::Resized, WindowResizedEvent{ window_size }
						);
						break;
					}
					case SDL_WINDOWEVENT_MAXIMIZED: {
						V2_int window_size{ e.window.data1, e.window.data2 };
						game.event.window.Post(
							WindowEvent::Maximized, WindowMaximizedEvent{ window_size }
						);
						break;
					}
					case SDL_WINDOWEVENT_MINIMIZED: {
						V2_int window_size{ e.window.data1, e.window.data2 };
						game.event.window.Post(
							WindowEvent::Minimized, WindowMinimizedEvent{ window_size }
						);
						break;
					}
					case SDL_WINDOWEVENT_MOVED: {
						V2_int window_pos{ e.window.data1, e.window.data2 };
						game.event.window.Post(WindowEvent::Moved, WindowMovedEvent{});
						break;
					}
					default: break;
				}
				break;
			}
			default: break;
		}
	}

	auto mouse_pressed_events = [&](Mouse mouse) {
		auto [mouse_state, timer] = GetMouseStateAndTimer(mouse);
		if (mouse_state == MouseState::Pressed) {
			game.event.mouse.Post(MouseEvent::Pressed, MousePressedEvent{ mouse });
		}
	};

	std::invoke(mouse_pressed_events, Mouse::Left);
	std::invoke(mouse_pressed_events, Mouse::Right);
	std::invoke(mouse_pressed_events, Mouse::Middle);
}

bool InputHandler::MouseWithinWindow() const {
	auto screen_pointer{ game.input.GetMousePositionGlobal() };
	Transform window_transform{ game.window.GetPosition() };
	Rect window_rect{ game.window.GetSize() };
	return Overlap(screen_pointer, window_transform, window_rect);
}

bool InputHandler::MouseHeld(Mouse button, milliseconds time) {
	const auto held_time{ GetMouseHeldTime(button) };
	return held_time > time;
}

void InputHandler::SetRelativeMouseMode(bool on) const {
	SDL_SetRelativeMouseMode(static_cast<SDL_bool>(on));
}

V2_float InputHandler::GetMousePositionGlobal() const {
	V2_int position;
	// SDL_PumpEvents not required as this function queries the OS directly.
	SDL_GetGlobalMouseState(&position.x, &position.y);
	return position;
}

V2_float InputHandler::GetMousePosition() const {
	return mouse_position_;
}

V2_float InputHandler::GetMousePositionUnclamped() const {
	return GetMousePositionGlobal() - game.window.GetPosition();
}

V2_float InputHandler::GetMousePositionPrevious() const {
	return previous_mouse_position_;
}

V2_float InputHandler::GetMouseDifference() const {
	return mouse_position_ - previous_mouse_position_;
}

int InputHandler::GetMouseScroll() const {
	return mouse_scroll_.y;
}

milliseconds InputHandler::GetMouseHeldTime(Mouse button) {
	auto [state, timer] = GetMouseStateAndTimer(button);
	// Retrieve held time in nanoseconds for maximum precision.
	const auto held_time{ timer.Elapsed<milliseconds>() };
	// Comparison units handled by chrono.
	return held_time;
}

inline static int WindowEventWatcher([[maybe_unused]] void* data, SDL_Event* event) {
	if (event->type == SDL_WINDOWEVENT) {
		if (event->window.event == SDL_WINDOWEVENT_RESIZED ||
			event->window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
			V2_int window_size{ event->window.data1, event->window.data2 };
			game.event.window.Post(WindowEvent::Resizing, WindowResizingEvent{ window_size });
		} else if (event->window.event == SDL_WINDOWEVENT_EXPOSED) {
			game.event.window.Post(WindowEvent::Drag, WindowDragEvent{});
		}
	}
	return 0;
}

void InputHandler::Init() {
	SDL_AddEventWatch(WindowEventWatcher, nullptr);
}

void InputHandler::Shutdown() {
	SDL_DelEventWatch(WindowEventWatcher, nullptr);
	Reset();
}

void InputHandler::UpdateMouseState(Mouse button) {
	auto [state, timer] = GetMouseStateAndTimer(button);
	if (timer.IsRunning() && state == MouseState::Down) {
		state = MouseState::Pressed;
	} else if (!timer.IsRunning() && state == MouseState::Up) {
		state = MouseState::Released;
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
	auto i{ static_cast<std::size_t>(key) };
	PTGN_ASSERT(i < InputHandler::key_count_, "Could not find key in input handler key states");
	return pressed_keys_[i];
}

bool InputHandler::KeyReleased(Key key) const {
	return !KeyPressed(key);
}

bool InputHandler::KeyDown(Key key) {
	auto i{ static_cast<std::size_t>(key) };
	PTGN_ASSERT(i < InputHandler::key_count_, "Could not find key in input handler key states");
	return pressed_keys_[i] && !previous_pressed_keys_[i];
}

bool InputHandler::KeyUp(Key key) {
	auto i{ static_cast<std::size_t>(key) };
	PTGN_ASSERT(i < InputHandler::key_count_, "Could not find key in input handler key states");
	return !pressed_keys_[i] && previous_pressed_keys_[i];
}

} // namespace ptgn::impl
