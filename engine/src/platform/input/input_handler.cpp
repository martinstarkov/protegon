#include "platform/input/input_handler.h"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_timer.h>

#include <array>
#include <chrono>
#include <utility>

#include "core/math/vector2.h"
#include "core/time/time.h"
#include "platform/input/events.h"
#include "platform/input/key.h"
#include "platform/input/mouse.h"
#include "platform/window/window.h"

namespace ptgn {

static milliseconds GetTimeSince(impl::Timestamp timestamp) {
	return milliseconds{ SDL_GetTicks() - timestamp };
}

InputHandler::InputHandler(Window& window) : window_{ window } {}

V2_float InputHandler::GetMousePosition() const {
	return mouse_position_;
}

V2_float InputHandler::GetPreviousMousePosition() const {
	return previous_mouse_position_;
}

V2_float InputHandler::GetMouseDelta() const {
	return GetMousePosition() - GetPreviousMousePosition();
}

float InputHandler::GetMouseScroll() const {
	return mouse_scroll_delta_.y;
}

bool InputHandler::MousePressed(Mouse mouse_button) const {
	return mouse_states_[std::to_underlying(mouse_button)] == MouseState::Pressed;
}

bool InputHandler::MouseReleased(Mouse mouse_button) const {
	return mouse_states_[std::to_underlying(mouse_button)] == MouseState::Released;
}

bool InputHandler::MouseHeld(Mouse mouse_button) const {
	return mouse_states_[std::to_underlying(mouse_button)] == MouseState::Held;
}

bool InputHandler::MouseHeld(Mouse mouse_button, milliseconds time) const {
	return GetMouseHeldTime(mouse_button) >= time;
}

milliseconds InputHandler::GetMouseHeldTime(Mouse button) const {
	return GetTimeSince(mouse_timestamps_[std::to_underlying(button)]);
}

bool InputHandler::KeyPressed(Key key) const {
	return key_states_[std::to_underlying(key)] == KeyState::Pressed;
}

bool InputHandler::KeyReleased(Key key) const {
	return key_states_[std::to_underlying(key)] == KeyState::Released;
}

bool InputHandler::KeyHeld(Key key) const {
	return key_states_[std::to_underlying(key)] == KeyState::Held;
}

bool InputHandler::KeyHeld(Key key, milliseconds time) const {
	return GetKeyHeldTime(key) >= time;
}

milliseconds InputHandler::GetKeyHeldTime(Key key) const {
	return GetTimeSince(key_timestamps_[std::to_underlying(key)]);
}

V2_float InputHandler::GetMouseScreenPosition() const {
	V2_float mouse_screen_pos;
	// SDL_PumpEvents not required as this function queries the OS directly.
	SDL_GetGlobalMouseState(&mouse_screen_pos.x, &mouse_screen_pos.y);
	V2_float window_pos{ window_.GetPosition() };
	mouse_screen_pos -= window_pos;
	return mouse_screen_pos;
}

void InputHandler::Update(const EventSink& sink) {
	previous_mouse_position_ = mouse_position_;
	mouse_scroll_			 = {};
	mouse_scroll_delta_		 = {};

	// Set key state from pressed to held and from released to idle to ensure those states only
	// last one frame.
	for (std::size_t i{ 0 }; i < key_states_.size(); ++i) {
		using enum KeyState;
		auto& state{ key_states_[i] };
		if (state == Released) {
			state			   = Idle;
			key_timestamps_[i] = SDL_GetTicks();
		} else if (state == Pressed) {
			state = Held;
		}
	}

	// Set mouse button states from pressed to held and from released to idle to ensure those states
	// only last one frame.
	for (std::size_t i{ 0 }; i < mouse_states_.size(); ++i) {
		using enum MouseState;
		auto& state{ mouse_states_[i] };
		if (state == Released) {
			state				 = Idle;
			mouse_timestamps_[i] = SDL_GetTicks();
		} else if (state == Pressed) {
			state = Held;
		}
	}

	PollEvents(sink);

	// Before polling events, their states are updated from pressed to held and from released to
	// idle. This means that if a key or mouse button is still held after polling events, it has
	// must have been held.
	for (std::size_t i{ 0 }; i < mouse_states_.size(); ++i) {
		if (mouse_states_[i] == MouseState::Held) {
			ptgn::MouseHeld held;
			held.button	  = static_cast<Mouse>(i);
			held.position = mouse_position_;
			sink(held);
		}
	}

	for (std::size_t i{ 0 }; i < key_states_.size(); ++i) {
		if (key_states_[i] == KeyState::Held) {
			ptgn::KeyHeld held;
			held.key = static_cast<Key>(i);
			sink(held);
		}
	}
}

void InputHandler::PollEvents(const EventSink& sink) {
	SDL_Event e;

	auto half_window_size{ window_.GetSize() / 2.0f };

	while (SDL_PollEvent(&e)) {
		switch (e.type) {
			case SDL_EVENT_MOUSE_MOTION: {
				mouse_position_ = V2_float{ e.motion.x, e.motion.y } - half_window_size;

				MouseMove move;
				move.position = mouse_position_;
				move.delta	  = { e.motion.xrel, e.motion.yrel };
				sink(move);
				break;
			}
			case SDL_EVENT_MOUSE_BUTTON_DOWN: {
				Mouse mouse{ static_cast<Mouse>(e.button.button) };
				auto index{ std::to_underlying(mouse) };

				mouse_timestamps_[index] = e.button.timestamp;
				mouse_states_[index]	 = MouseState::Pressed;

				ptgn::MousePressed pressed;
				pressed.button	 = mouse;
				pressed.position = mouse_position_;
				sink(pressed);

				ptgn::MouseHeld held;
				held.button	  = mouse;
				held.position = mouse_position_;
				sink(held);
				break;
			}
			case SDL_EVENT_MOUSE_BUTTON_UP: {
				Mouse mouse{ static_cast<Mouse>(e.button.button) };
				auto index{ std::to_underlying(mouse) };

				mouse_timestamps_[index] = e.button.timestamp;
				mouse_states_[index]	 = MouseState::Released;

				ptgn::MouseReleased released;
				released.button	  = mouse;
				released.position = mouse_position_;
				sink(released);
				break;
			}
			case SDL_EVENT_KEY_DOWN: {
				auto index{ static_cast<std::size_t>(e.key.scancode) };
				Key key{ static_cast<Key>(index) };

				key_timestamps_[index] = e.key.timestamp;
				key_states_[index]	   = KeyState::Pressed;

				ptgn::KeyPressed pressed;
				pressed.key = key;
				sink(pressed);

				ptgn::KeyHeld held;
				held.key = key;
				sink(held);
				break;
			}
			case SDL_EVENT_KEY_UP: {
				auto index{ static_cast<std::size_t>(e.key.scancode) };
				Key key{ static_cast<Key>(index) };

				key_timestamps_[index] = e.key.timestamp;
				key_states_[index]	   = KeyState::Released;

				ptgn::KeyReleased released;
				released.key = key;
				sink(released);
				break;
			}
			case SDL_EVENT_MOUSE_WHEEL: {
				mouse_scroll_timestamp_	 = e.wheel.timestamp;
				mouse_scroll_			 = { e.wheel.x, e.wheel.y };
				mouse_scroll_delta_		+= mouse_scroll_;

				ptgn::MouseScroll scroll;
				scroll.scroll	= mouse_scroll_;
				scroll.position = mouse_position_;
				sink(scroll);
				break;
			}
			case SDL_EVENT_QUIT: {
				WindowQuit quit{};
				sink(quit);
				break;
			}
			case SDL_EVENT_WINDOW_RESIZED: {
				WindowResized resized;
				resized.size = { e.window.data1, e.window.data2 };
				sink(resized);
				break;
			}
			case SDL_EVENT_WINDOW_MAXIMIZED: {
				WindowMaximized maximized;
				maximized.size = { e.window.data1, e.window.data2 };
				sink(maximized);
				break;
			}
			case SDL_EVENT_WINDOW_MINIMIZED: {
				WindowMinimized minimized;
				minimized.size = { e.window.data1, e.window.data2 };
				sink(minimized);
				break;
			}
			case SDL_EVENT_WINDOW_MOVED: {
				WindowMoved moved;
				moved.position = { e.window.data1, e.window.data2 };
				sink(moved);
				break;
			}
			case SDL_EVENT_WINDOW_FOCUS_LOST: {
				WindowFocusLost focus{};
				sink(focus);
				break;
			}
			case SDL_EVENT_WINDOW_FOCUS_GAINED: {
				WindowFocusGained focus{};
				sink(focus);
				break;
			}
			default: break;
		}
	}

	V2_float new_mouse_position;
	// TODO: Consider using global mouse position here in the future.
	// I can foresee a bug where mouse position difference is zero if the user alt+tabs to
	// lose window focus and then regains it via alt+tab while the mouse is technically in the same
	// location. This would result in no MouseMove event being queued, which may make certain
	// scripts function incorrectly. But I'm not sure to be honest, so I won't change it.
	SDL_GetMouseState(&new_mouse_position.x, &new_mouse_position.y);

	V2_float difference{ new_mouse_position - mouse_position_ };

	if (!difference.IsZero()) {
		mouse_position_ = new_mouse_position - half_window_size;

		ptgn::MouseMove move;
		move.position = mouse_position_;
		move.delta	  = difference;
		sink(move);
	}
}

} // namespace ptgn
