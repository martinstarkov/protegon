#include "event/input_handler.h"

#include <bitset>
#include <utility>

#include "SDL_events.h"
#include "SDL_keyboard.h"
#include "SDL_mouse.h"
#include "SDL_stdinc.h"
#include "SDL_video.h"
#include "core/game.h"
#include "core/window.h"
#include "event/event_handler.h"
#include "event/events.h"
#include "event/key.h"
#include "event/mouse.h"
#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "renderer/origin.h"
#include "renderer/render_target.h"
#include "renderer/renderer.h"
#include "scene/camera.h"
#include "scene/scene_manager.h"
#include "utility/debug.h"
#include "utility/log.h"
#include "utility/time.h"
#include "utility/timer.h"

namespace ptgn::impl {

void InputHandler::ResetKeyStates() {
	key_states_.reset();
	first_time_down_.reset();
	first_time_up_.reset();
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
	mouse_pos_		= {};
	prev_mouse_pos_ = {};
	mouse_scroll_	= {};
}

void InputHandler::Reset() {
	ResetKeyStates();
	ResetMouseStates();
	ResetMousePositions();
}

void InputHandler::Update() {
	prev_mouse_pos_ = mouse_pos_;
	// Update mouse states.
	UpdateMouseState(Mouse::Left);
	UpdateMouseState(Mouse::Right);
	UpdateMouseState(Mouse::Middle);
	mouse_scroll_ = {};
	SDL_Event e;
	while (SDL_PollEvent(&e)) {
		switch (e.type) {
			case SDL_MOUSEMOTION: {
				mouse_pos_.x = e.motion.x;
				mouse_pos_.y = e.motion.y;
				game.event.mouse.Post(MouseEvent::Move, MouseMoveEvent{});
				break;
			}
			case SDL_MOUSEBUTTONDOWN: {
				auto [mouse_state, timer] =
					GetMouseStateAndTimer(static_cast<Mouse>(e.button.button));
				timer.Start();
				mouse_state = MouseState::Down;
				game.event.mouse.Post(
					MouseEvent::Down, MouseDownEvent{ static_cast<Mouse>(e.button.button) }
				);
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
}

bool InputHandler::MouseWithinWindow() const {
	Rect window{ game.window.GetPosition(), game.window.GetSize(), Origin::TopLeft };

	return window.Overlaps(game.input.GetMousePositionGlobal());
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

V2_float InputHandler::GetMousePositionWindow() const {
	return mouse_pos_;
}

V2_float InputHandler::GetMousePositionPreviousWindow() const {
	return prev_mouse_pos_;
}

V2_float InputHandler::GetMouseDifferenceWindow() const {
	return mouse_pos_ - prev_mouse_pos_;
}

V2_float InputHandler::GetMouseDifference(RenderTarget render_target) const {
	if (!render_target.IsValid()) {
		render_target = game.renderer.GetRenderTarget();
	}
	if (render_target.IsValid()) {
		return ScreenToViewport(
			render_target.GetViewport(), render_target.GetCamera(), GetMouseDifferenceWindow()
		);
	}
	return ScreenToViewport(
		game.renderer.GetViewport(), game.camera.GetPrimary(), GetMouseDifferenceWindow()
	);
}

V2_float InputHandler::GetMousePosition(RenderTarget render_target) const {
	if (!render_target.IsValid()) {
		render_target = game.renderer.GetRenderTarget();
	}
	return ScreenToViewport(
		render_target.GetViewport(), render_target.GetCamera(), GetMousePositionWindow()
	);
}

V2_float InputHandler::GetMousePositionPrevious(RenderTarget render_target) const {
	if (!render_target.IsValid()) {
		render_target = game.renderer.GetRenderTarget();
	}
	if (render_target.IsValid()) {
		return ScreenToViewport(
			render_target.GetViewport(), render_target.GetCamera(), GetMousePositionPreviousWindow()
		);
	}
	return ScreenToViewport(
		game.renderer.GetViewport(), game.camera.GetPrimary(), GetMousePositionPreviousWindow()
	);
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

} // namespace ptgn::impl
