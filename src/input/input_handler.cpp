#include "input/input_handler.h"

#include <chrono>
#include <optional>
#include <type_traits>
#include <variant>

#include "SDL_events.h"
#include "SDL_keyboard.h"
#include "SDL_mouse.h"
#include "SDL_stdinc.h"
#include "SDL_timer.h"
#include "SDL_video.h"
#include "common/assert.h"
#include "components/transform.h"
#include "core/game.h"
#include "core/time.h"
#include "core/window.h"
#include "debug/log.h"
#include "input/events.h"
#include "input/key.h"
#include "input/mouse.h"
#include "math/geometry/rect.h"
#include "math/overlap.h"
#include "math/vector2.h"

namespace ptgn::impl {

void InputHandler::ResetKeyStates() {
	for (std::size_t i{ 0 }; i < key_states_.size(); ++i) {
		if (key_states_[i] == KeyState::Up) {
			key_timestamps_[i] = SDL_GetTicks();
			key_states_[i]	   = KeyState::Released;
		}
	}
}

void InputHandler::ResetMouseStates() {
	for (std::size_t i{ 0 }; i < mouse_states_.size(); ++i) {
		if (mouse_states_[i] == MouseState::Up) {
			mouse_timestamps_[i] = SDL_GetTicks();
			mouse_states_[i]	 = MouseState::Released;
		}
	}
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

std::optional<InputEvent> InputHandler::GetInputEvent(const SDL_Event& e) {
	switch (e.type) {
		case SDL_MOUSEMOTION: {
			V2_int new_mouse_position{ e.motion.x, e.motion.y };
			mouse_position_ = new_mouse_position;
			return MouseMove{ new_mouse_position, V2_int{ e.motion.xrel, e.motion.yrel } };
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
			mouse_position_			= new_mouse_position;
			mouse_scroll_timestamp_ = e.wheel.timestamp;
			mouse_scroll_			= { e.wheel.x, e.wheel.y };
			return MouseScroll{ mouse_scroll_, mouse_position_ };
		}
		case SDL_QUIT: {
			return WindowQuit{};
		}
		case SDL_WINDOWEVENT: {
			switch (e.window.event) {
				case SDL_WINDOWEVENT_RESIZED:
				case SDL_WINDOWEVENT_SIZE_CHANGED: {
					V2_int window_size{ e.window.data1, e.window.data2 };
					return WindowResized{ window_size };
				}
				case SDL_WINDOWEVENT_MAXIMIZED: {
					V2_int window_size{ e.window.data1, e.window.data2 };
					return WindowMaximized{ window_size };
				}
				case SDL_WINDOWEVENT_MINIMIZED: {
					V2_int window_size{ e.window.data1, e.window.data2 };
					return WindowMinimized{ window_size };
				}
				case SDL_WINDOWEVENT_MOVED: {
					V2_int window_position{ e.window.data1, e.window.data2 };
					return WindowMoved{ window_position };
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
	SDL_GetMouseState(&new_mouse_position.x, &new_mouse_position.y);

	V2_int difference{ new_mouse_position - mouse_position_ };

	if (!difference.IsZero()) {
		mouse_position_ = new_mouse_position;
		queue_.emplace_back(MouseMove{ mouse_position_, difference });
	}
}

void InputHandler::Prepare() {
	ResetMouseStates();
	ResetKeyStates();

	previous_mouse_position_ = mouse_position_;
	mouse_scroll_			 = {};
	queue_.clear();

	for (auto& mouse_state : mouse_states_) {
		if (mouse_state == MouseState::Down) {
			mouse_state = MouseState::Pressed;
		}
	}
}

void InputHandler::DispatchInputEvents() {
	for (const auto& event : queue_) {
		std::visit(
			[&](auto&& ev) {
				using T = std::decay_t<decltype(ev)>;

				// --- Keyboard Events ---
				if constexpr (std::is_same_v<T, impl::KeyDown>) {
					// PTGN_LOG("KeyDown: ", ev.key);
					// PTGN_LOG("KeyPressed: ", ev.key);
				} else if constexpr (std::is_same_v<T, impl::KeyPressed>) {
					// PTGN_LOG("KeyPressed: ", ev.key);
				} else if constexpr (std::is_same_v<T, impl::KeyUp>) {
					// PTGN_LOG("KeyUp: ", ev.key);
				}

				// --- Mouse Events ---
				else if constexpr (std::is_same_v<T, impl::MouseMove>) {
					// PTGN_LOG("MouseMove: ", ev.position);
				} else if constexpr (std::is_same_v<T, impl::MouseDown>) {
					// PTGN_LOG("MouseDown: ", ev.button, ", pos: ", ev.position);
				} else if constexpr (std::is_same_v<T, impl::MousePressed>) {
					// PTGN_LOG("MousePressed: ", ev.button, ", pos: ", ev.position);
				} else if constexpr (std::is_same_v<T, impl::MouseUp>) {
					// PTGN_LOG("MouseUp: ", ev.button, ", pos: ", ev.position);
				} else if constexpr (std::is_same_v<T, impl::MouseScroll>) {
					// PTGN_LOG("MouseScroll: ", ev.scroll, ", pos: ", ev.position);
				}

				// --- Window Events ---
				else if constexpr (std::is_same_v<T, impl::WindowResized>) {
					// PTGN_LOG("WindowResized: (", ev.size.x, ", ", ev.size.y, ")");
				} else if constexpr (std::is_same_v<T, impl::WindowMoved>) {
					// PTGN_LOG("WindowMoved: (", ev.position.x, ", ", ev.position.y, ")");
				} else if constexpr (std::is_same_v<T, impl::WindowMaximized>) {
					// PTGN_LOG("WindowMaximized");
				} else if constexpr (std::is_same_v<T, impl::WindowMinimized>) {
					// PTGN_LOG("WindowMinimized");
				} else if constexpr (std::is_same_v<T, impl::WindowFocusLost>) {
					// PTGN_LOG("WindowFocusLost");
				} else if constexpr (std::is_same_v<T, impl::WindowFocusGained>) {
					// PTGN_LOG("WindowFocusGained");
				} else if constexpr (std::is_same_v<T, impl::WindowQuit>) {
					// PTGN_LOG("WindowQuit");
				}
			},
			event
		);
	}
}

void InputHandler::Update() {
	Prepare();
	ProcessInputEvents();
	DispatchInputEvents();
}

bool InputHandler::MouseWithinWindow() const {
	auto screen_pointer{ game.input.GetMousePositionGlobal() };
	Transform window_transform{ game.window.GetPosition() };
	Rect window_rect{ game.window.GetSize() };
	return Overlap(screen_pointer, window_transform, window_rect);
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
	Reset();
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

bool InputHandler::KeyDown(Key key) {
	return GetKeyState(key) == KeyState::Down;
}

bool InputHandler::KeyUp(Key key) {
	return GetKeyState(key) == KeyState::Up;
}

} // namespace ptgn::impl
