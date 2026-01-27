#include "platform/input/input_handler.h"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_timer.h>

#include <array>
#include <chrono>

#include "app/context.h"
#include "core/assert.h"
#include "core/event/event_handler.h"
#include "core/event/events.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "platform/input/key.h"
#include "platform/input/mouse.h"
#include "platform/window/resolution.h"
#include "platform/window/window.h"

namespace ptgn {

void InputHandler::EmitEvents() {
	SDL_Event e;

	while (SDL_PollEvent(&e)) {
		switch (e.type) {
			case SDL_EVENT_MOUSE_MOTION: {
				V2_float new_mouse_position{ e.motion.x, e.motion.y };
				mouse_position_ = new_mouse_position;

				ptgn::MouseMove move;
				move.position	= new_mouse_position;
				move.difference = { e.motion.xrel, e.motion.yrel };

				ctx_->events.Emit(move);
				break;
			}
			case SDL_EVENT_MOUSE_BUTTON_DOWN: {
				Mouse mouse{ static_cast<Mouse>(e.button.button) };

				ptgn::MouseDown down;

				down.button	  = mouse;
				down.position = mouse_position_;

				if (auto index{ GetMouseIndex(mouse) };
					mouse_states_[index] != MouseState::Pressed) {
					mouse_timestamps_[index] = e.button.timestamp;
					mouse_states_[index]	 = MouseState::Down;
					down.held				 = false;
				} else {
					down.held = true;
				}

				ctx_->events.Emit(down);
				break;
			}
			case SDL_EVENT_MOUSE_BUTTON_UP: {
				Mouse mouse{ static_cast<Mouse>(e.button.button) };

				if (auto index{ GetMouseIndex(mouse) };
					mouse_states_[index] != MouseState::Released) {
					ptgn::MouseUp up;

					up.button	= mouse;
					up.position = mouse_position_;

					mouse_timestamps_[index] = e.button.timestamp;
					mouse_states_[index]	 = MouseState::Up;

					ctx_->events.Emit(up);
				}

				break;
			}
			case SDL_EVENT_KEY_DOWN: {
				auto index{ static_cast<std::size_t>(e.key.scancode) };
				Key key{ static_cast<Key>(index) };

				ptgn::KeyDown down;
				down.key = key;

				if (!e.key.repeat) {
					key_timestamps_[index] = e.key.timestamp;
					key_states_[index]	   = KeyState::Down;
					down.held			   = false;
				} else {
					down.held = true;
				}

				ctx_->events.Emit(down);
				break;
			}
			case SDL_EVENT_KEY_UP: {
				if (auto index{ static_cast<std::size_t>(e.key.scancode) };
					key_states_[index] != KeyState::Released) {
					key_timestamps_[index] = e.key.timestamp;
					key_states_[index]	   = KeyState::Up;

					ptgn::KeyUp up;
					up.key = static_cast<Key>(index);

					ctx_->events.Emit(up);
				}
				break;
			}
			case SDL_EVENT_MOUSE_WHEEL: {
				V2_float new_mouse_position{ e.wheel.mouse_x, e.wheel.mouse_y };
				mouse_position_			 = new_mouse_position;
				mouse_scroll_timestamp_	 = e.wheel.timestamp;
				mouse_scroll_			 = { e.wheel.x, e.wheel.y };
				mouse_scroll_delta_		+= mouse_scroll_;

				ptgn::MouseScroll scroll;
				scroll.scroll	= mouse_scroll_;
				scroll.position = mouse_position_;

				ctx_->events.Emit(scroll);
				break;
			}
			case SDL_EVENT_QUIT: {
				ctx_->events.Emit(WindowQuit{});
				ctx_->Stop();
				break;
			}
			case SDL_EVENT_WINDOW_RESIZED: {
				WindowResized resized;

				resized.size = { e.window.data1, e.window.data2 };

				ctx_->events.Emit(resized);
				break;
			}
			case SDL_EVENT_WINDOW_MAXIMIZED: {
				WindowMaximized maximized;

				maximized.size = { e.window.data1, e.window.data2 };

				ctx_->events.Emit(maximized);
				break;
			}
			case SDL_EVENT_WINDOW_MINIMIZED: {
				WindowMinimized minimized;

				minimized.size = { e.window.data1, e.window.data2 };

				ctx_->events.Emit(minimized);
				break;
			}
			case SDL_EVENT_WINDOW_MOVED: {
				WindowMoved moved;

				moved.position = { e.window.data1, e.window.data2 };

				ctx_->events.Emit(moved);
				break;
			}
			case SDL_EVENT_WINDOW_FOCUS_LOST: {
				ctx_->events.Emit(WindowFocusLost{});
				break;
			}
			case SDL_EVENT_WINDOW_FOCUS_GAINED: {
				ctx_->events.Emit(WindowFocusGained{});
				break;
			}
			default: break;
		}
	}

	for (std::size_t i{ 0 }; i < mouse_states_.size(); ++i) {
		const auto& mouse_state{ mouse_states_[i] };
		if (mouse_state == MouseState::Pressed) {
			Mouse mouse{ GetMouse(i) };

			ptgn::MouseDown down;

			down.button	  = mouse;
			down.held	  = true;
			down.position = mouse_position_;

			ctx_->events.Emit(down);
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
		mouse_position_ = new_mouse_position;

		ptgn::MouseMove move;

		move.position	= mouse_position_;
		move.difference = difference;

		ctx_->events.Emit(move);
	}
}

void InputHandler::Update() {
	for (std::size_t i{ 0 }; i < key_states_.size(); ++i) {
		if (key_states_[i] == KeyState::Up) {
			key_timestamps_[i] = SDL_GetTicks();
			key_states_[i]	   = KeyState::Released;
		}
	}
	for (std::size_t i{ 0 }; i < mouse_states_.size(); ++i) {
		if (mouse_states_[i] == MouseState::Up) {
			mouse_timestamps_[i] = SDL_GetTicks();
			mouse_states_[i]	 = MouseState::Released;
		}
	}

	previous_mouse_position_ = mouse_position_;
	mouse_scroll_			 = {};
	mouse_scroll_delta_		 = {};

	for (auto& mouse_state : mouse_states_) {
		if (mouse_state == MouseState::Down) {
			mouse_state = MouseState::Pressed;
		}
	}
	for (auto& key_state : key_states_) {
		if (key_state == KeyState::Down) {
			key_state = KeyState::Pressed;
		}
	}

	EmitEvents();
}

V2_float InputHandler::GetPositionRelativeTo(
	V2_float window_position, ViewportType relative_to, bool clamp_to_viewport
) const {
	// TODO: Move into a resolution manager.
	/*
	V2_float window_center{ window_.GetSize() / 2 };

	V2_float window_point{ window_position };

	// Make position relative to the center of the window.
	window_point -= window_center;

	switch (relative_to) {
		case ViewportType::World: {
			auto game_scale{ renderer_.GetScale() };
			auto current{ scenes_.GetCurrent() };
			PTGN_ASSERT(current != nullptr);
			auto rt_transform{ GetTransform(current->GetRenderTarget()) };
			return WindowToWorld(game_scale, rt_transform, window_point, current->camera);
		}
		case ViewportType::Game: {
			auto game_scale{ renderer_.GetScale() };
			auto game_point{ WindowToGame(game_scale, window_point) };
			if (clamp_to_viewport) {
				auto game_size{ renderer_.GetGameSize() };
				auto half_size{ game_size * 0.5f };
				game_point = Clamp(game_point, -half_size, half_size);
			}
			return game_point;
		}
		case ViewportType::Display: {
			auto display_point{ WindowToDisplay(window_point) };
			if (clamp_to_viewport) {
				auto display_size{ renderer_.GetDisplaySize() };
				auto half_size{ display_size * 0.5f };
				display_point = Clamp(display_point, -half_size, half_size);
			}
			return display_point;
		}
		case ViewportType::WindowCenter:  return window_point;
		case ViewportType::WindowTopLeft: return window_position;
		default:						  PTGN_ERROR("Unrecognized viewport type");
	}*/
	return window_position;
}

void InputHandler::SetContext(const std::shared_ptr<ApplicationContext>& ctx) {
	ctx_ = ctx;
}

V2_float InputHandler::GetMouseScreenPosition() const {
	V2_float mouse_screen_pos;
	// SDL_PumpEvents not required as this function queries the OS directly.
	SDL_GetGlobalMouseState(&mouse_screen_pos.x, &mouse_screen_pos.y);
	V2_float window_pos{ ctx_->window.GetPosition() };
	mouse_screen_pos -= window_pos;
	return mouse_screen_pos;
}

V2_float InputHandler::GetMousePosition(ViewportType relative_to, bool clamp_to_viewport) const {
	V2_float mouse_window_pos{ mouse_position_ };

	if (!clamp_to_viewport) {
		mouse_window_pos = GetMouseScreenPosition();
	}

	return GetPositionRelativeTo(mouse_window_pos, relative_to, clamp_to_viewport);
}

V2_float InputHandler::GetMousePositionPrevious(ViewportType relative_to, bool clamp_to_viewport)
	const {
	return GetPositionRelativeTo(previous_mouse_position_, relative_to, clamp_to_viewport);
}

V2_float InputHandler::GetMousePositionDifference(ViewportType relative_to, bool clamp_to_viewport)
	const {
	return GetMousePosition(relative_to, clamp_to_viewport) -
		   GetMousePositionPrevious(relative_to, clamp_to_viewport);
}

int InputHandler::GetMouseScroll() const {
	return mouse_scroll_delta_.y;
}

milliseconds InputHandler::GetTimeSince(Timestamp timestamp) {
	Timestamp current{ SDL_GetTicks() };
	PTGN_ASSERT(current >= timestamp, "Timestamp cannot be in the future");
	return milliseconds{ current - timestamp };
}

milliseconds InputHandler::GetMouseHeldTime(Mouse mouse_button) const {
	auto index{ GetMouseIndex(mouse_button) };
	auto mouse_timestamp{ mouse_timestamps_[index] };
	auto mouse_state{ mouse_states_[index] };
	if (!mouse_timestamp || mouse_state == MouseState::Up || mouse_state == MouseState::Released) {
		return milliseconds{ 0 };
	}
	return GetTimeSince(mouse_timestamp);
}

milliseconds InputHandler::GetKeyHeldTime(Key key) const {
	auto index{ GetKeyIndex(key) };
	auto key_timestamp{ key_timestamps_[index] };
	auto key_state{ key_states_[index] };
	if (!key_timestamp || key_state == KeyState::Up || key_state == KeyState::Released) {
		return milliseconds{ 0 };
	}
	return GetTimeSince(key_timestamp);
}

InputHandler::KeyState InputHandler::GetKeyState(Key key) const {
	auto index{ GetKeyIndex(key) };
	return key_states_[index];
}

InputHandler::Timestamp InputHandler::GetKeyTimestamp(Key key) const {
	auto index{ GetKeyIndex(key) };
	return key_timestamps_[index];
}

InputHandler::MouseState InputHandler::GetMouseState(Mouse mouse_button) const {
	auto index{ GetMouseIndex(mouse_button) };
	return mouse_states_[index];
}

InputHandler::Timestamp InputHandler::GetMouseTimestamp(Mouse mouse_button) const {
	auto index{ GetMouseIndex(mouse_button) };
	return mouse_timestamps_[index];
}

std::size_t InputHandler::GetKeyIndex(Key key) const {
	return static_cast<std::size_t>(key);
}

std::size_t InputHandler::GetMouseIndex(Mouse mouse_button) const {
	std::size_t index{ 0 };
	switch (mouse_button) {
		using enum ptgn::Mouse;
		case Left:	 index = 0; break;
		case Right:	 index = 1; break;
		case Middle: index = 2; break;
		default:	 PTGN_ERROR("Unknown mouse button");
	}
	PTGN_ASSERT(index < mouse_states_.size());
	return index;
}

Mouse InputHandler::GetMouse(std::size_t mouse_index) const {
	switch (mouse_index) {
		using enum ptgn::Mouse;
		case 0:	 return Left;
		case 1:	 return Right;
		case 2:	 return Middle;
		default: PTGN_ERROR("Unknown mouse index");
	}
}

bool InputHandler::MouseHeld(Mouse mouse_button, milliseconds time) const {
	auto held_time{ GetMouseHeldTime(mouse_button) };
	return held_time >= time;
}

bool InputHandler::MousePressed(Mouse mouse_button) const {
	auto state{ GetMouseState(mouse_button) };
	return state == MouseState::Pressed || state == MouseState::Down;
}

bool InputHandler::MouseReleased(Mouse mouse_button) const {
	auto state{ GetMouseState(mouse_button) };
	return state == MouseState::Released || state == MouseState::Up;
}

bool InputHandler::MouseDown(Mouse mouse_button) const {
	return GetMouseState(mouse_button) == MouseState::Down;
}

bool InputHandler::MouseUp(Mouse mouse_button) const {
	return GetMouseState(mouse_button) == MouseState::Up;
}

bool InputHandler::KeyHeld(Key key, milliseconds time) const {
	auto held_time{ GetKeyHeldTime(key) };
	return held_time >= time;
}

bool InputHandler::KeyPressed(Key key) const {
	auto state{ GetKeyState(key) };
	return state == KeyState::Pressed || state == KeyState::Down;
}

bool InputHandler::KeyReleased(Key key) const {
	auto state{ GetKeyState(key) };
	return state == KeyState::Released || state == KeyState::Up;
}

bool InputHandler::KeyDown(Key key) const {
	return GetKeyState(key) == KeyState::Down;
}

bool InputHandler::KeyUp(Key key) const {
	return GetKeyState(key) == KeyState::Up;
}

} // namespace ptgn
