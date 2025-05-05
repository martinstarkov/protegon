#pragma once

#include <bitset>
#include <utility>

#include "event/key.h"
#include "event/mouse.h"
#include "math/vector2.h"
#include "utility/time.h"
#include "utility/timer.h"

union SDL_Event;

namespace ptgn {

class Scene;
class SceneInput;

namespace impl {

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

	// TODO: Add KeyHeld().

	// @param button The mouse button to check.
	// @return The amount of time that the mouse button has been held down.
	[[nodiscard]] milliseconds GetMouseHeldTime(Mouse button);

	/*
	 * @param button The mouse button to check.
	 * @param time The duration of time for which the mouse should be held.
	 * @return True if the mouse button has been held for the given amount of time.
	 */
	[[nodiscard]] bool MouseHeld(Mouse button, milliseconds time = milliseconds{ 50 });

	// @return True if mouse position is within window bounds, false otherwise.
	[[nodiscard]] bool MouseWithinWindow() const;

	// While the mouse is in relative mode, the cursor is hidden, the mouse position is constrained
	// to the window, and there will be continuous relative mouse motion events triggered even if
	// the mouse is at the edge of the window.
	// @param Whether or not mouse relative mode should be turned on or not.
	void SetRelativeMouseMode(bool on) const;

	// @return Mouse position relative to the top left of the window.
	[[nodiscard]] V2_float GetMousePosition() const;

	// @return Mouse position during the previous frame relative to the top left of the window.
	[[nodiscard]] V2_float GetMousePositionPrevious() const;

	// @return Mouse position difference between the current and previous frames relative to the top
	// left of the window.
	[[nodiscard]] V2_float GetMouseDifference() const;

	// @return In desktop mode: mouse position relative to the screen (display). In browser: same as
	// GetMousePosition().
	[[nodiscard]] V2_float GetMousePositionGlobal() const;

	// @return The amount scrolled by the mouse vertically in the current frame,
	// positive upward, negative downward. Zero if no scroll occurred.
	[[nodiscard]] int GetMouseScroll() const;

	// @param button The mouse button to check.
	// @return True if the mouse button is pressed (true every frame that the button is down).
	[[nodiscard]] bool MousePressed(Mouse button) const;

	// @param button The mouse button to check.
	// @return True if the mouse button is released (true every frame that the button is up).
	[[nodiscard]] bool MouseReleased(Mouse button) const;

	// @param button The mouse button to check.
	// @return True the first frame that the mouse button is pressed (false every frame after that).
	[[nodiscard]] bool MouseDown(Mouse button) const;

	// @param button The mouse button to check.
	// @return True the first frame that the mouse button is released (false every frame after
	// that).
	[[nodiscard]] bool MouseUp(Mouse button) const;

	// @param button The key to check.
	// @return True if the key is pressed (true every frame that the key is down).
	[[nodiscard]] bool KeyPressed(Key key) const;

	// @param button The key to check.
	// @return True if the key is released (true every frame that the key is up).
	[[nodiscard]] bool KeyReleased(Key key) const;

	// @param button The key to check.
	// @return True the first frame that the key is pressed (false every frame after that).
	[[nodiscard]] bool KeyDown(Key key);

	// @param button The key to check.
	// @return True the first frame that the key is released (false every frame after that).
	[[nodiscard]] bool KeyUp(Key key);

private:
	friend class ptgn::Scene;
	friend class SceneManager;
	friend class Game;
	friend class ptgn::SceneInput;

	// Updates the user inputs and posts any triggered input events. Run internally when using game
	// scenes.
	void Update();

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

	void Reset();
	void ResetKeyStates();
	void ResetMouseStates();
	void ResetMousePositions();

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

} // namespace impl

} // namespace ptgn
