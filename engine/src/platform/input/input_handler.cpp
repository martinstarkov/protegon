#include "platform/input/input_handler.h"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_timer.h>

#include <array>
#include <chrono>
#include <utility>

#include "core/assert.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "platform/input/events.h"
#include "platform/input/key.h"
#include "platform/input/mouse.h"
#include "platform/window/window.h"
#include "renderer/renderer.h"
#include "runtime/event/event_handler.h"

namespace ptgn {

static nanoseconds GetTimeSince(impl::Timestamp timestamp) {
	return nanoseconds{ SDL_GetTicksNS() - timestamp };
}

static Mouse GetMouse(const SDL_MouseButtonEvent& event) {
	auto mouse{ event.button - 1 };
	PTGN_ASSERT(mouse >= 0 && mouse < impl::kMouseCount, "Mouse button not supported: ", mouse);
	return static_cast<Mouse>(mouse);
}

static Key GetKey(const SDL_KeyboardEvent& event) {
	PTGN_ASSERT(
		event.scancode >= 0 && event.scancode < impl::kKeyCount,
		"Key scancode is not supported: ", event.scancode
	);
	return static_cast<Key>(event.scancode);
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
	return mouse_states_[std::to_underlying(mouse_button)] == impl::MouseState::Pressed;
}

bool InputHandler::MouseReleased(Mouse mouse_button) const {
	return mouse_states_[std::to_underlying(mouse_button)] == impl::MouseState::Released;
}

bool InputHandler::MouseHeld(Mouse mouse_button) const {
	return mouse_states_[std::to_underlying(mouse_button)] == impl::MouseState::Held;
}

bool InputHandler::MouseHeld(Mouse mouse_button, milliseconds time) const {
	return GetMouseHeldTime(mouse_button) >= time;
}

milliseconds InputHandler::GetMouseHeldTime(Mouse button) const {
	return duration_cast<milliseconds>(GetTimeSince(mouse_timestamps_[std::to_underlying(button)]));
}

bool InputHandler::KeyPressed(Key key) const {
	return key_states_[std::to_underlying(key)] == impl::KeyState::Pressed;
}

bool InputHandler::KeyReleased(Key key) const {
	return key_states_[std::to_underlying(key)] == impl::KeyState::Released;
}

bool InputHandler::KeyHeld(Key key) const {
	auto state{ key_states_[std::to_underlying(key)] };
	return state == impl::KeyState::Held || state == impl::KeyState::Pressed;
}

bool InputHandler::KeyHeld(Key key, milliseconds time) const {
	return GetKeyHeldTime(key) >= time;
}

milliseconds InputHandler::GetKeyHeldTime(Key key) const {
	return duration_cast<milliseconds>(GetTimeSince(key_timestamps_[std::to_underlying(key)]));
}

V2_float InputHandler::GetMouseScreenPosition() const {
	V2_float mouse_screen_pos;
	// SDL_PumpEvents not required as this function queries the OS directly.
	SDL_GetGlobalMouseState(&mouse_screen_pos.x, &mouse_screen_pos.y);
	V2_float window_pos{ window_.GetPosition() };
	mouse_screen_pos -= window_pos;
	return mouse_screen_pos;
}

void InputHandler::ClearInputState() {
	for (std::size_t i = 0; i < key_states_.size(); ++i) {
		key_states_[i]	   = impl::KeyState::Idle;
		key_timestamps_[i] = SDL_GetTicksNS();
	}

	for (std::size_t i = 0; i < mouse_states_.size(); ++i) {
		mouse_states_[i]	 = impl::MouseState::Idle;
		mouse_timestamps_[i] = SDL_GetTicksNS();
	}

	mouse_scroll_		= {};
	mouse_scroll_delta_ = {};
}

bool InputHandler::Update(EventHandler& events, Renderer& renderer) {
	previous_mouse_position_ = mouse_position_;
	mouse_scroll_			 = {};
	mouse_scroll_delta_		 = {};

	// Set key state from pressed to held and from released to idle to ensure those states only
	// last one frame.
	for (std::size_t i{ 0 }; i < key_states_.size(); ++i) {
		using enum impl::KeyState;
		auto& state{ key_states_[i] };
		if (state == Released) {
			state			   = Idle;
			key_timestamps_[i] = SDL_GetTicksNS();
		} else if (state == Pressed) {
			state = Held;
		}
	}

	// Set mouse button states from pressed to held and from released to idle to ensure those states
	// only last one frame.
	for (std::size_t i{ 0 }; i < mouse_states_.size(); ++i) {
		using enum impl::MouseState;
		auto& state{ mouse_states_[i] };
		if (state == Released) {
			state				 = Idle;
			mouse_timestamps_[i] = SDL_GetTicksNS();
		} else if (state == Pressed) {
			state = Held;
		}
	}

	bool running{ PollEvents(events, renderer) };

	if (window_.focused_) {
		// Before polling events, their states are updated from pressed to held and from released to
		// idle. This means that if a key or mouse button is still held after polling events, it has
		// must have been held.
		for (std::size_t i{ 0 }; i < mouse_states_.size(); ++i) {
			if (mouse_states_[i] == impl::MouseState::Held) {
				events.Push<event::MouseHeld>(static_cast<Mouse>(i), mouse_position_);
			}
		}

		for (std::size_t i{ 0 }; i < key_states_.size(); ++i) {
			if (key_states_[i] == impl::KeyState::Held) {
				events.Push<event::KeyHeld>(static_cast<Key>(i));
			}
		}
	}

	return running;
}

bool InputHandler::PollEvents(EventHandler& events, Renderer& renderer) {
	SDL_Event e;

	V2_float half_window_size{ window_.GetSize() / 2.0f };

	bool mouse_moved{ false };

	bool running{ true };

	while (SDL_PollEvent(&e)) {
		switch (e.type) {
			case SDL_EVENT_MOUSE_MOTION: {
				mouse_position_ = V2_float{ e.motion.x, e.motion.y } - half_window_size;
				mouse_moved		= true;
				mouse_set_		= true;
				events.Push<event::MouseMove>(
					mouse_position_, V2_int{ e.motion.xrel, e.motion.yrel }
				);
				break;
			}
			case SDL_EVENT_MOUSE_BUTTON_DOWN: {
				Mouse mouse{ GetMouse(e.button) };
				auto index{ std::to_underlying(mouse) };

				mouse_timestamps_[index] = e.button.timestamp;
				mouse_states_[index]	 = impl::MouseState::Pressed;

				events.Push<event::MousePressed>(mouse, mouse_position_);
				events.Push<event::MouseHeld>(mouse, mouse_position_);

				break;
			}
			case SDL_EVENT_MOUSE_BUTTON_UP: {
				Mouse mouse{ GetMouse(e.button) };
				auto index{ std::to_underlying(mouse) };

				mouse_timestamps_[index] = e.button.timestamp;
				mouse_states_[index]	 = impl::MouseState::Released;

				events.Push<event::MouseReleased>(mouse, mouse_position_);
				break;
			}
			case SDL_EVENT_KEY_DOWN: {
				Key key{ GetKey(e.key) };
				auto index{ std::to_underlying(key) };

				if (!e.key.repeat) {
					key_timestamps_[index] = e.key.timestamp;
					key_states_[index]	   = impl::KeyState::Pressed;
				} else {
					key_states_[index] = impl::KeyState::Held;
				}

				events.Push<event::KeyPressed>(key);
				events.Push<event::KeyHeld>(key);
				break;
			}
			case SDL_EVENT_KEY_UP: {
				Key key{ GetKey(e.key) };
				auto index{ std::to_underlying(key) };

				key_timestamps_[index] = e.key.timestamp;
				key_states_[index]	   = impl::KeyState::Released;

				events.Push<event::KeyReleased>(key);
				break;
			}
			case SDL_EVENT_MOUSE_WHEEL: {
				mouse_scroll_timestamp_	 = e.wheel.timestamp;
				mouse_scroll_			 = { e.wheel.x, e.wheel.y };
				mouse_scroll_delta_		+= mouse_scroll_;

				events.Push<event::MouseScroll>(mouse_scroll_, mouse_position_);
				break;
			}
			case SDL_EVENT_QUIT: {
				events.Push<event::WindowQuit>();
				running = false;
				break;
			}
			case SDL_EVENT_WINDOW_RESIZED: {
				V2_int size{ e.window.data1, e.window.data2 };
				renderer.OnWindowResize(size);
				events.Push<event::WindowResized>(size);
				break;
			}
			case SDL_EVENT_WINDOW_MAXIMIZED: {
				V2_int size{ e.window.data1, e.window.data2 };
				events.Push<event::WindowMaximized>(size);
				break;
			}
			case SDL_EVENT_WINDOW_MINIMIZED: {
				V2_int size{ e.window.data1, e.window.data2 };
				events.Push<event::WindowMinimized>(size);
				break;
			}
			case SDL_EVENT_WINDOW_MOVED: {
				V2_int position{ e.window.data1, e.window.data2 };
				events.Push<event::WindowMoved>(position);
				break;
			}
			case SDL_EVENT_WINDOW_FOCUS_LOST: {
				window_.focused_ = false;
				ClearInputState();
				events.Push<event::WindowFocusLost>();
				break;
			}
			case SDL_EVENT_WINDOW_FOCUS_GAINED: {
				window_.focused_ = true;
				events.Push<event::WindowFocusGained>();
				break;
			}
			default: break;
		}
	}

	if (window_.focused_ && !mouse_moved) {
		V2_float global_mouse_position;
		// If mouse moves outside the window, SDL does not send a mouse motion event, so we query
		// manually.

		auto window_position{ window_.GetPosition() };

#ifdef __EMSCRIPTEN__
		SDL_GetMouseState(&global_mouse_position.x, &global_mouse_position.y);
		window_position = {};
#else
		SDL_GetGlobalMouseState(&global_mouse_position.x, &global_mouse_position.y);
#endif

		V2_float new_mouse_position{ global_mouse_position - window_position - half_window_size };

		V2_float difference{ new_mouse_position - mouse_position_ };

		// Mouse moved outside the window.
		if (!difference.IsZero()) {
			mouse_position_ = new_mouse_position;

			if (mouse_set_) {
				events.Push<event::MouseMove>(mouse_position_, difference);
			}

			mouse_set_ = true;
		}
	}

	return running;
}

} // namespace ptgn
