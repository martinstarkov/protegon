#pragma once

#include <array>
#include <bitset>
#include <cstdint>
#include <optional>
#include <utility>

#include "core/time.h"
#include "core/timer.h"
#include "events.h"
#include "input/key.h"
#include "input/mouse.h"
#include "math/vector2.h"

union SDL_Event;

namespace ptgn {

class Scene;
class SceneInput;

namespace impl {

class Game;
class SceneManager;

class InputHandler {
public:
	InputHandler()									 = default;
	~InputHandler() noexcept						 = default;
	InputHandler(const InputHandler&)				 = delete;
	InputHandler& operator=(const InputHandler&)	 = delete;
	InputHandler(InputHandler&&) noexcept			 = default;
	InputHandler& operator=(InputHandler&&) noexcept = default;

	// @param mouse_button The mouse button to check.
	// @return The amount of time that the mouse button has been held down, 0 if it is not currently
	// pressed.
	[[nodiscard]] milliseconds GetMouseHeldTime(Mouse mouse_button) const;

	// @param key The key to check.
	// @return The amount of time that the key has been held down, 0 if it is not currently pressed.
	[[nodiscard]] milliseconds GetKeyHeldTime(Key key) const;

	/*
	 * @param mouse_button The mouse button to check.
	 * @param time The duration of time for which the mouse should be held.
	 * @return True if the mouse button has been held for the given amount of time.
	 */
	[[nodiscard]] bool MouseHeld(Mouse mouse_button, milliseconds time = milliseconds{ 50 }) const;

	/*
	 * @param key The key to check.
	 * @param time The duration of time for which the key should be held.
	 * @return True if the key has been held for the given amount of time.
	 */
	[[nodiscard]] bool KeyHeld(Key key, milliseconds time = milliseconds{ 50 }) const;

	// @return True if mouse position is within window bounds, false otherwise.
	[[nodiscard]] bool MouseWithinWindow() const;

	// While the mouse is in relative mode, the cursor is hidden, the mouse position is constrained
	// to the window, and there will be continuous relative mouse motion events triggered even if
	// the mouse is at the edge of the window.
	// @param Whether or not mouse relative mode should be turned on or not.
	void SetRelativeMouseMode(bool on) const;

	// @return Mouse position relative to the top left of the window, clamped to the range [0,
	// window_size].
	[[nodiscard]] V2_float GetMousePosition() const;

	// @return Mouse position relative to the top left of the window, without clamping to the range
	// [0, window_size].
	[[nodiscard]] V2_float GetMousePositionUnclamped() const;

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

	// @param mouse_button The mouse button to check.
	// @return True if the mouse button is pressed (true every frame that the button is down).
	[[nodiscard]] bool MousePressed(Mouse mouse_button) const;

	// @param mouse_button The mouse button to check.
	// @return True if the mouse button is released (true every frame that the button is up).
	[[nodiscard]] bool MouseReleased(Mouse mouse_button) const;

	// @param mouse_button The mouse button to check.
	// @return True the first frame that the mouse button is pressed (false every frame after that).
	[[nodiscard]] bool MouseDown(Mouse mouse_button) const;

	// @param mouse_button The mouse button to check.
	// @return True the first frame that the mouse button is released (false every frame after
	// that).
	[[nodiscard]] bool MouseUp(Mouse mouse_button) const;

	// @param mouse_button The key to check.
	// @return True if the key is pressed (true every frame that the key is down).
	[[nodiscard]] bool KeyPressed(Key key) const;

	// @param mouse_button The key to check.
	// @return True if the key is released (true every frame that the key is up).
	[[nodiscard]] bool KeyReleased(Key key) const;

	// @param mouse_button The key to check.
	// @return True the first frame that the key is pressed (false every frame after that).
	[[nodiscard]] bool KeyDown(Key key);

	// @param mouse_button The key to check.
	// @return True the first frame that the key is released (false every frame after that).
	[[nodiscard]] bool KeyUp(Key key);

private:
	friend class ptgn::Scene;
	friend class SceneManager;
	friend class Game;
	friend class ptgn::SceneInput;

	using Timestamp = std::uint32_t;

	// Updates the user inputs and posts any triggered input events. Run internally when using game
	// scenes.
	void Update();

	[[nodiscard]] MouseState GetMouseState(Mouse mouse_button) const;
	[[nodiscard]] Timestamp GetMouseTimestamp(Mouse mouse_button) const;

	[[nodiscard]] KeyState GetKeyState(Key key) const;
	[[nodiscard]] Timestamp GetKeyTimestamp(Key key) const;

	[[nodiscard]] std::size_t GetKeyIndex(Key key) const;
	[[nodiscard]] std::size_t GetMouseIndex(Mouse mouse_button) const;
	[[nodiscard]] Mouse GetMouse(std::size_t mouse_index) const;

	[[nodiscard]] std::optional<InputEvent> GetInputEvent(const SDL_Event& e);

	void Init();
	void Shutdown();

	void Reset();
	void ResetKeyStates();
	void ResetMouseStates();
	void ResetMousePositions();

	[[nodiscard]] static milliseconds GetTimeSince(Timestamp timestamp);

	// Number of keys stored in the SDL key states array.
	static constexpr std::size_t key_count_{ 512 };

	static constexpr std::size_t mouse_count_{ 3 };

	std::array<KeyState, key_count_> key_states_;
	std::array<Timestamp, key_count_> key_timestamps_;
	std::array<MouseState, mouse_count_> mouse_states_;
	std::array<Timestamp, mouse_count_> mouse_timestamps_;

	V2_int mouse_position_;
	V2_int previous_mouse_position_;

	Timestamp mouse_scroll_timestamp_{ 0 };
	V2_int mouse_scroll_;

	void Prepare();
	void ProcessInputEvents();
	void DispatchInputEvents();

	InputQueue queue_;
};

} // namespace impl

} // namespace ptgn
