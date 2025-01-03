#pragma once

#include <bitset>
#include <utility>

#include "event/key.h"
#include "event/mouse.h"
#include "math/vector2.h"
#include "utility/time.h"
#include "utility/timer.h"

union SDL_Event;

namespace ptgn::impl {

class Game;
class SceneManager;

class InputHandler {
public:
	InputHandler()								 = default;
	~InputHandler()								 = default;
	InputHandler(const InputHandler&)			 = delete;
	InputHandler(InputHandler&&)				 = default;
	InputHandler& operator=(const InputHandler&) = delete;
	InputHandler& operator=(InputHandler&&)		 = default;

private:
	// Updates previous mouse states for mouse up and down check.
	void UpdateMouseState(Mouse button);

	/*
	 * @param button Mouse enum corresponding to the desired button.
	 * @return Pair of pointers to the mouse state and timer for a given button,
	 * pair of nullptrs if no such button exists.
	 */
	[[nodiscard]] std::pair<MouseState&, Timer&> GetMouseStateAndTimer(Mouse button);

	/*
	 * @param button Mouse enum corresponding to the desired button.
	 * @return Current state of the given mouse button.
	 */
	[[nodiscard]] MouseState GetMouseState(Mouse button) const;

	void Init();
	void Shutdown();

public:
	// Updates the user inputs and posts any triggered events.
	void Update();

	[[nodiscard]] milliseconds GetMouseHeldTime(Mouse button);

	/*
	 * @tparam Duration The unit of time measurement.
	 * @return True if the mouse button has been held for the given amount of time.
	 */
	template <typename Duration = milliseconds, tt::duration<Duration> = true>
	[[nodiscard]] inline bool MouseHeld(Mouse button, Duration time = milliseconds{ 50 }) {
		const auto held_time{ GetMouseHeldTime(button) };
		return held_time > time;
	}

	// @return True if mouse position is within window bounds, false otherwise.
	bool MouseWithinWindow() const;

	void SetRelativeMouseMode(bool on) const;

	// @return Mouse position relative to the top left of the window.
	[[nodiscard]] V2_float GetMousePositionWindow() const;

	// @return Mouse position during the previous frame relative to the top left of the window.
	[[nodiscard]] V2_float GetMousePositionPreviousWindow() const;

	// @return Mouse position difference between the current and previous frames relative to the top
	// left of the window.
	[[nodiscard]] V2_float GetMouseDifferenceWindow() const;

	// @return Mouse position scaled relative to the camera size of the specified render layer.
	[[nodiscard]] V2_float GetMousePosition(std::size_t render_layer = 0) const;

	// @return Mouse position during the previous frame scaled relative to the camera size of the
	// specified render layer.
	[[nodiscard]] V2_float GetMousePositionPrevious(std::size_t render_layer = 0) const;

	// @return Mouse position difference between the current and previous frames scaled relative to
	// the camera size of the specified render layer.
	[[nodiscard]] V2_float GetMouseDifference(std::size_t render_layer = 0) const;

	// @return In desktop mode: mouse position relative to the screen (display). In browser: same as
	// GetMousePosition().
	[[nodiscard]] V2_float GetMousePositionGlobal() const;

	// @return The amount scrolled by the mouse vertically in the current frame,
	// positive upward, negative downward. Zero if no scroll occurred.
	[[nodiscard]] int GetMouseScroll() const;

	[[nodiscard]] bool MousePressed(Mouse button) const;
	[[nodiscard]] bool MouseReleased(Mouse button) const;
	[[nodiscard]] bool MouseDown(Mouse button) const;
	[[nodiscard]] bool MouseUp(Mouse button) const;

	[[nodiscard]] bool KeyPressed(Key key) const;
	[[nodiscard]] bool KeyReleased(Key key) const;
	[[nodiscard]] bool KeyDown(Key key);
	[[nodiscard]] bool KeyUp(Key key);

private:
	[[nodiscard]] V2_float ScaledToRenderLayer(const V2_float& position, std::size_t render_layer)
		const;

	friend class SceneManager;
	friend class Game;

	void Reset();

	// Number of keys stored in the SDL key states array. For creating previous
	// key states array.
	static constexpr std::size_t key_count_{ 512 };

	// Previous loop cycle key states for comparison with current.
	std::bitset<key_count_> key_states_;
	std::bitset<key_count_> first_time_down_;
	std::bitset<key_count_> first_time_up_;

	// Mouse states.
	MouseState left_mouse_{ MouseState::Released };
	MouseState right_mouse_{ MouseState::Released };
	MouseState middle_mouse_{ MouseState::Released };
	V2_int mouse_pos_;
	V2_int prev_mouse_pos_;
	V2_int mouse_scroll_;

	// Mouse button held for timers.

	Timer left_mouse_timer_;
	Timer right_mouse_timer_;
	Timer middle_mouse_timer_;
};

} // namespace ptgn::impl
