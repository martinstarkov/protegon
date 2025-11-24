#pragma once

#include <array>
#include <cstdint>
#include <memory>

#include "core/app/resolution.h"
#include "core/input/key.h"
#include "core/input/mouse.h"
#include "core/util/time.h"
#include "math/vector2.h"

namespace ptgn {

class Application;
class ApplicationContext;

class InputHandler {
public:
	InputHandler()									 = default;
	~InputHandler() noexcept						 = default;
	InputHandler(const InputHandler&)				 = delete;
	InputHandler& operator=(const InputHandler&)	 = delete;
	InputHandler(InputHandler&&) noexcept			 = delete;
	InputHandler& operator=(InputHandler&&) noexcept = delete;

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
	//[[nodiscard]] bool MouseWithinWindow() const;

	// While the mouse is in relative mode, the cursor is hidden, the mouse position is constrained
	// to the window, and there will be continuous relative mouse motion events triggered even if
	// the mouse is at the edge of the window.
	// @param Whether or not mouse relative mode should be turned on or not.
	void SetRelativeMouseMode(bool on) const;

	// @return Mouse position.
	[[nodiscard]] V2_float GetMousePosition(
		ViewportType relative_to = ViewportType::World, bool clamp_to_viewport = true
	) const;

	// @return Mouse position during the previous frame.
	[[nodiscard]] V2_float GetMousePositionPrevious(
		ViewportType relative_to = ViewportType::World, bool clamp_to_viewport = true
	) const;

	// @return Mouse position difference between the current and previous frames.
	[[nodiscard]] V2_float GetMousePositionDifference(
		ViewportType relative_to = ViewportType::World, bool clamp_to_viewport = true
	) const;

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

	// @param key The key to check.
	// @return True if the key is pressed (true every frame that the key is down).
	[[nodiscard]] bool KeyPressed(Key key) const;

	// @param key The key to check.
	// @return True if the key is released (true every frame that the key is up).
	[[nodiscard]] bool KeyReleased(Key key) const;

	// @param key The key to check.
	// @return True the first frame that the key is pressed (false every frame after that).
	[[nodiscard]] bool KeyDown(Key key) const;

	// @param key The key to check.
	// @return True the first frame that the key is released (false every frame after that).
	[[nodiscard]] bool KeyUp(Key key) const;

private:
	friend class Application;

	using Timestamp = std::uint32_t;

	enum class KeyState : std::uint8_t {
		Up		 = 1,
		Down	 = 2,
		Released = 3,
		Pressed	 = 4
	};

	enum class MouseState : std::uint8_t {
		Up		 = 1,
		Down	 = 2,
		Released = 3,
		Pressed	 = 4
	};

	// Convert position from being relative to the top left of the window to being relative to the
	// center of the specified viewport.
	[[nodiscard]] V2_float GetPositionRelativeTo(
		V2_int window_position, ViewportType relative_to, bool clamp_to_viewport
	) const;

	// @return Mouse position relative to the top left of the screen.
	[[nodiscard]] V2_int GetMouseScreenPosition() const;

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

	[[nodiscard]] static milliseconds GetTimeSince(Timestamp timestamp);

	void EmitEvents();

	// Number of keys stored in the SDL key states array.
	static constexpr std::size_t key_count_{ 512 };

	static constexpr std::size_t mouse_count_{ 3 };

	std::array<KeyState, key_count_> key_states_{};
	std::array<Timestamp, key_count_> key_timestamps_{};
	std::array<MouseState, mouse_count_> mouse_states_{};
	std::array<Timestamp, mouse_count_> mouse_timestamps_{};

	// Stored mouse positions are relative to the top left of the window.
	V2_int mouse_position_;
	V2_int previous_mouse_position_;

	// Total scroll amount in the current frame (cumulative).
	V2_int mouse_scroll_delta_;
	// Scroll amount in the most recent scroll event.
	V2_int mouse_scroll_;
	// Timestamp of the most recent scroll event.
	Timestamp mouse_scroll_timestamp_{ 0 };

	void SetContext(const std::shared_ptr<ApplicationContext>& ctx);

	std::shared_ptr<ApplicationContext> ctx_;
};

PTGN_SERIALIZE_ENUM(
	InputHandler::MouseState, { { InputHandler::MouseState::Up, "up" },
								{ InputHandler::MouseState::Down, "down" },
								{ InputHandler::MouseState::Released, "released" },
								{ InputHandler::MouseState::Pressed, "pressed" } }
);

PTGN_SERIALIZE_ENUM(
	InputHandler::KeyState, { { InputHandler::KeyState::Up, "up" },
							  { InputHandler::KeyState::Down, "down" },
							  { InputHandler::KeyState::Released, "released" },
							  { InputHandler::KeyState::Pressed, "pressed" } }
);

} // namespace ptgn
