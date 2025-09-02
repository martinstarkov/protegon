#include "input/input_handler.h"

#include <chrono>
#include <optional>
#include <type_traits>
#include <variant>
#include <vector>

#include "common/assert.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/resolution.h"
#include "core/time.h"
#include "core/window.h"
#include "debug/log.h"
#include "input/events.h"
#include "input/key.h"
#include "input/mouse.h"
#include "math/geometry/rect.h"
#include "math/overlap.h"
#include "math/vector2.h"
#include "renderer/render_data.h"
#include "renderer/renderer.h"
#include "scene/scene.h"
#include "SDL_events.h"
#include "SDL_keyboard.h"
#include "SDL_mouse.h"
#include "SDL_stdinc.h"
#include "SDL_timer.h"
#include "SDL_video.h"

namespace ptgn::impl {

std::optional<InputEvent> InputHandler::GetInputEvent(const SDL_Event& e) {
	switch (e.type) {
		case SDL_MOUSEMOTION: {
			V2_int new_mouse_position{ e.motion.x, e.motion.y };
			mouse_position_ = new_mouse_position;
			// V2_int difference{ e.motion.xrel, e.motion.yrel };
			return MouseMove{};
		}
		case SDL_MOUSEBUTTONDOWN: {
			Mouse mouse{ static_cast<Mouse>(e.button.button) };
			auto index{ GetMouseIndex(mouse) };
			if (mouse_states_[index] != MouseState::Pressed) {
				mouse_timestamps_[index] = e.button.timestamp;
				mouse_states_[index]	 = MouseState::Down;
				return impl::MouseDown{ mouse };
			}
			return impl::MousePressed{ mouse };
		}
		case SDL_MOUSEBUTTONUP: {
			Mouse mouse{ static_cast<Mouse>(e.button.button) };
			auto index{ GetMouseIndex(mouse) };
			if (mouse_states_[index] != MouseState::Released) {
				mouse_timestamps_[index] = e.button.timestamp;
				mouse_states_[index]	 = MouseState::Up;
				return impl::MouseUp{ mouse };
			}
			break;
		}
		case SDL_KEYDOWN: {
			auto index{ static_cast<std::size_t>(e.key.keysym.scancode) };
			Key key{ static_cast<Key>(index) };
			if (!e.key.repeat) {
				key_timestamps_[index] = e.key.timestamp;
				key_states_[index]	   = KeyState::Down;
				return impl::KeyDown{ key };
			}
			return impl::KeyPressed{ key };
		}
		case SDL_KEYUP: {
			auto index{ static_cast<std::size_t>(e.key.keysym.scancode) };
			if (key_states_[index] != KeyState::Released) {
				key_timestamps_[index] = e.key.timestamp;
				key_states_[index]	   = KeyState::Up;
				Key key{ static_cast<Key>(index) };
				return impl::KeyUp{ key };
			}
			break;
		}
		case SDL_MOUSEWHEEL: {
			V2_int new_mouse_position{ e.wheel.mouseX, e.wheel.mouseY };
			mouse_position_			 = new_mouse_position;
			mouse_scroll_timestamp_	 = e.wheel.timestamp;
			mouse_scroll_			 = { e.wheel.x, e.wheel.y };
			mouse_scroll_delta_		+= mouse_scroll_;
			return MouseScroll{ mouse_scroll_ };
		}
		case SDL_QUIT: {
			return WindowQuit{};
		}
		case SDL_WINDOWEVENT: {
			switch (e.window.event) {
				case SDL_WINDOWEVENT_RESIZED:
				case SDL_WINDOWEVENT_SIZE_CHANGED: {
					// V2_int window_size{ e.window.data1, e.window.data2 };
					return WindowResized{};
				}
				case SDL_WINDOWEVENT_MAXIMIZED: {
					//	V2_int window_size{ e.window.data1, e.window.data2 };
					return WindowMaximized{};
				}
				case SDL_WINDOWEVENT_MINIMIZED: {
					//	V2_int window_size{ e.window.data1, e.window.data2 };
					return WindowMinimized{};
				}
				case SDL_WINDOWEVENT_MOVED: {
					//	V2_int window_position{ e.window.data1, e.window.data2 };
					return WindowMoved{};
				}
				case SDL_WINDOWEVENT_FOCUS_LOST: {
					return WindowFocusLost{};
				}
				case SDL_WINDOWEVENT_FOCUS_GAINED: {
					return WindowFocusGained{};
				}
				default: break;
			}
			break;
		}
		default: break;
	}
	return std::nullopt;
}

void InputHandler::ProcessInputEvents() {
	SDL_Event e;

	while (SDL_PollEvent(&e)) {
		if (auto event{ GetInputEvent(e) }) {
			queue_.emplace_back(*event);
		}
	}

	for (std::size_t i{ 0 }; i < mouse_states_.size(); ++i) {
		const auto& mouse_state{ mouse_states_[i] };
		if (mouse_state == MouseState::Pressed) {
			Mouse mouse_button{ GetMouse(i) };
			queue_.emplace_back(impl::MousePressed{ mouse_button });
		}
	}

	V2_int new_mouse_position;
	// TODO: Consider using global mouse position here in the future.
	// I can foresee a bug where mouse position difference is zero if the user alt+tabs to
	// lose window focus and then regains it via alt+tab while the mouse is technically in the same
	// location. This would result in no MouseMove event being queued, which may make certain
	// scripts function incorrectly. But I'm not sure to be honest, so I won't change it.
	SDL_GetMouseState(&new_mouse_position.x, &new_mouse_position.y);

	V2_int difference{ new_mouse_position - mouse_position_ };

	if (!difference.IsZero()) {
		mouse_position_ = new_mouse_position;
		queue_.emplace_back(MouseMove{});
	}
}

void InputHandler::Prepare() {
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
	queue_.clear();

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
}

void InputHandler::InvokeInputEvents(Manager& manager) {
	for (const auto& event : queue_) {
		std::visit(
			[&](auto&& ev) {
				using T = std::decay_t<decltype(ev)>;

				// Mouse events.
				if constexpr (std::is_same_v<T, impl::MouseMove>) {
					for (auto [e, scripts] : manager.EntitiesWith<Scripts>()) {
						scripts.AddAction(&GlobalMouseScript::OnMouseMove);
					}
				} else if constexpr (std::is_same_v<T, impl::MouseDown>) {
					for (auto [e, scripts] : manager.EntitiesWith<Scripts>()) {
						scripts.AddAction(&GlobalMouseScript::OnMouseDown, ev.button);
					}
				} else if constexpr (std::is_same_v<T, impl::MousePressed>) {
					for (auto [e, scripts] : manager.EntitiesWith<Scripts>()) {
						scripts.AddAction(&GlobalMouseScript::OnMousePressed, ev.button);
					}
				} else if constexpr (std::is_same_v<T, impl::MouseUp>) {
					for (auto [e, scripts] : manager.EntitiesWith<Scripts>()) {
						scripts.AddAction(&GlobalMouseScript::OnMouseUp, ev.button);
					}
				} else if constexpr (std::is_same_v<T, impl::MouseScroll>) {
					for (auto [e, scripts] : manager.EntitiesWith<Scripts>()) {
						scripts.AddAction(&GlobalMouseScript::OnMouseScroll, ev.scroll);
					}
				}

				// Keyboard events.
				if constexpr (std::is_same_v<T, impl::KeyDown>) {
					for (auto [e, scripts] : manager.EntitiesWith<Scripts>()) {
						scripts.AddAction(&KeyScript::OnKeyDown, ev.key);
						scripts.AddAction(&KeyScript::OnKeyPressed, ev.key);
					}
				} else if constexpr (std::is_same_v<T, impl::KeyPressed>) {
					for (auto [e, scripts] : manager.EntitiesWith<Scripts>()) {
						scripts.AddAction(&KeyScript::OnKeyPressed, ev.key);
					}
				} else if constexpr (std::is_same_v<T, impl::KeyUp>) {
					for (auto [e, scripts] : manager.EntitiesWith<Scripts>()) {
						scripts.AddAction(&KeyScript::OnKeyUp, ev.key);
					}
				}

				// Window events.
				else if constexpr (std::is_same_v<T, impl::WindowResized>) {
					for (auto [e, scripts] : manager.EntitiesWith<Scripts>()) {
						scripts.AddAction(&WindowScript::OnWindowResized);
					}
				} else if constexpr (std::is_same_v<T, impl::WindowMoved>) {
					for (auto [e, scripts] : manager.EntitiesWith<Scripts>()) {
						scripts.AddAction(&WindowScript::OnWindowMoved);
					}
				} else if constexpr (std::is_same_v<T, impl::WindowMaximized>) {
					for (auto [e, scripts] : manager.EntitiesWith<Scripts>()) {
						scripts.AddAction(&WindowScript::OnWindowMaximized);
					}
				} else if constexpr (std::is_same_v<T, impl::WindowMinimized>) {
					for (auto [e, scripts] : manager.EntitiesWith<Scripts>()) {
						scripts.AddAction(&WindowScript::OnWindowMinimized);
					}
				} else if constexpr (std::is_same_v<T, impl::WindowFocusLost>) {
					for (auto [e, scripts] : manager.EntitiesWith<Scripts>()) {
						scripts.AddAction(&WindowScript::OnWindowFocusLost);
					}
				} else if constexpr (std::is_same_v<T, impl::WindowFocusGained>) {
					for (auto [e, scripts] : manager.EntitiesWith<Scripts>()) {
						scripts.AddAction(&WindowScript::OnWindowFocusGained);
					}
				} else if constexpr (std::is_same_v<T, impl::WindowQuit>) {
					game.Stop();
				}
			},
			event
		);
	}

	for (auto [e, scripts] : manager.EntitiesWith<Scripts>()) {
		scripts.InvokeActions();
	}

	manager.Refresh();
}

void InputHandler::Update() {
	Prepare();
	ProcessInputEvents();
}

bool InputHandler::MouseWithinWindow() const {
	auto screen_pointer{ GetMouseScreenPosition() };
	auto window_size{ game.window.GetSize() };
	auto window_position{ game.window.GetPosition() };
	Transform window_transform{ window_position + window_size / 2 };
	Rect window_rect{ window_size };
	return Overlap(screen_pointer, window_transform, window_rect);
}

void InputHandler::SetRelativeMouseMode(bool on) const {
	SDL_SetRelativeMouseMode(static_cast<SDL_bool>(on));
}

V2_float InputHandler::GetPositionRelativeTo(
	const V2_int& window_position, ViewportType relative_to
) {
	V2_int window_center{ game.window.GetSize() / 2 };

	V2_int window_point{ window_position };

	// Make position relative to the center of the window.
	window_point -= window_center;

	switch (relative_to) {
		case ViewportType::World:	return WindowToWorld(window_point, {});
		case ViewportType::Game:	return WindowToGame(window_point);
		case ViewportType::Display: return WindowToDisplay(window_point);
		default:					PTGN_ERROR("Unrecognized viewport type")
	}
}

V2_int InputHandler::GetMouseScreenPosition() const {
	V2_int mouse_screen_pos;
	// SDL_PumpEvents not required as this function queries the OS directly.
	SDL_GetGlobalMouseState(&mouse_screen_pos.x, &mouse_screen_pos.y);
	V2_int window_pos{ game.window.GetPosition() };
	mouse_screen_pos -= window_pos;
	return mouse_screen_pos;
}

V2_float InputHandler::GetMousePosition(ViewportType relative_to, bool clamp_to_viewport) const {
	V2_int mouse_window_pos{ mouse_position_ };

	if (!clamp_to_viewport) {
		mouse_window_pos = GetMouseScreenPosition();
	}

	return GetPositionRelativeTo(mouse_window_pos, relative_to);
}

V2_float InputHandler::GetMousePositionPrevious(ViewportType relative_to) const {
	return GetPositionRelativeTo(previous_mouse_position_, relative_to);
}

V2_float InputHandler::GetMousePositionDifference(ViewportType relative_to) const {
	return GetMousePosition(relative_to, true) - GetMousePositionPrevious(relative_to);
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

// inline static int WindowEventWatcher([[maybe_unused]] void* data, SDL_Event* event) {
//	if (event->type == SDL_WINDOWEVENT) {
//		if (event->window.event == SDL_WINDOWEVENT_RESIZED ||
//			event->window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
//			V2_int window_size{ event->window.data1, event->window.data2 };
//			// TODO: This is not safe due to being on a different thread.
//			queue.emplace_back(WindowResizing{ window_size });
//		} else if (event->window.event == SDL_WINDOWEVENT_EXPOSED) {
//			// TODO: This is not safe due to being on a different thread.
//			queue.emplace_back(WindowDrag{});
//		}
//	}
//	return 0;
// }

void InputHandler::Init() {
	// SDL_AddEventWatch(WindowEventWatcher, nullptr);
}

void InputHandler::Shutdown() {
	// SDL_DelEventWatch(WindowEventWatcher, nullptr);
}

KeyState InputHandler::GetKeyState(Key key) const {
	auto index{ GetKeyIndex(key) };
	return key_states_[index];
}

InputHandler::Timestamp InputHandler::GetKeyTimestamp(Key key) const {
	auto index{ GetKeyIndex(key) };
	return key_timestamps_[index];
}

MouseState InputHandler::GetMouseState(Mouse mouse_button) const {
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
		case Mouse::Left:	index = 0; break;
		case Mouse::Right:	index = 1; break;
		case Mouse::Middle: index = 2; break;
		default:			PTGN_ERROR("Unknown mouse button");
	}
	PTGN_ASSERT(index < mouse_states_.size());
	return index;
}

Mouse InputHandler::GetMouse(std::size_t mouse_index) const {
	switch (mouse_index) {
		case 0:	 return Mouse::Left;
		case 1:	 return Mouse::Right;
		case 2:	 return Mouse::Middle;
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

} // namespace ptgn::impl
