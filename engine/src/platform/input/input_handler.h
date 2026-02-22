#pragma once

#include <array>
#include <cstdint>
#include <functional>

#include "core/event/event.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "platform/input/key.h"
#include "platform/input/mouse.h"
#include "serialization/json/enum.h"

namespace ptgn {

class Application;
class Window;
class SceneInput;

namespace impl {

using Timestamp = std::uint64_t;

} // namespace impl

class InputHandler {
public:
	explicit InputHandler(Window& window);
	~InputHandler() noexcept						 = default;
	InputHandler(const InputHandler&)				 = delete;
	InputHandler& operator=(const InputHandler&)	 = delete;
	InputHandler(InputHandler&&) noexcept			 = delete;
	InputHandler& operator=(InputHandler&&) noexcept = delete;

	/// @return Mouse position relative to the center of the window.
	[[nodiscard]] V2_float GetMousePosition() const;

	/// @return Mouse position relative to the center of the window during the previous frame.
	[[nodiscard]] V2_float GetPreviousMousePosition() const;

	/// @return Mouse delta (current_position - previous_position) relative to the center of the
	/// window.
	[[nodiscard]] V2_float GetMouseDelta() const;

	/// @return The amount scrolled by the mouse vertically in the current frame,
	/// positive upward, negative downward. Zero if no scroll occurred.
	[[nodiscard]] float GetMouseScroll() const;

	/// @param button The mouse button to check.
	/// @return True the first frame that the mouse is pressed.
	[[nodiscard]] bool MousePressed(Mouse button) const;

	/// @param button The mouse button to check.
	/// @return True the first frame that the mouse button is released.
	[[nodiscard]] bool MouseReleased(Mouse button) const;

	/// @param button The mouse button to check.
	/// @return True every frame that the mouse button is pressed.
	[[nodiscard]] bool MouseHeld(Mouse button) const;

	/// @param button The mouse button to check.
	/// @param time The duration of time for which the mouse should be held.
	/// @return True if the mouse button has been held for the given amount of time.
	[[nodiscard]] bool MouseHeld(Mouse button, milliseconds time) const;

	/// @param button The mouse button to check.
	/// @return The amount of time that the mouse button has been held down, negative numbers
	/// indicate the time since the mouse button was last held.
	[[nodiscard]] milliseconds GetMouseHeldTime(Mouse button) const;

	/// @param key The key to check.
	/// @return True the first frame that the key is pressed.
	[[nodiscard]] bool KeyPressed(Key key) const;

	/// @param key The key to check.
	/// @return True the first frame that the key is released.
	[[nodiscard]] bool KeyReleased(Key key) const;

	/// @param key The key to check.
	/// @return True every frame that the key is pressed.
	[[nodiscard]] bool KeyHeld(Key key) const;

	/// @param key The key to check.
	/// @param time The duration of time for which the key should be held.
	/// @return True if the key has been held for the given amount of time.
	[[nodiscard]] bool KeyHeld(Key key, milliseconds time) const;

	/// @param key The key to check.
	/// @return The amount of time that the key has been held down, negative numbers
	/// indicate the time since the key was last held.
	[[nodiscard]] milliseconds GetKeyHeldTime(Key key) const;

private:
	friend class Application;
	friend class SceneInput;

	using EventSink = std::function<void(impl::EventBase&)>;

	enum class KeyState : std::uint8_t {
		Idle	 = 0, /// When the key is not pressed.
		Pressed	 = 1, /// First frame that the key is pressed.
		Held	 = 2, /// Every subsequent frame that the key is pressed.
		Released = 3, /// First frame that the key is released.
	};

	enum class MouseState : std::uint8_t {
		Idle	 = 0, /// When the mouse button is not pressed.
		Pressed	 = 1, /// First frame that the mouse button is pressed.
		Held	 = 2, /// Every subsequent frame that the mouse button is pressed.
		Released = 3, /// First frame that the mouse button is released.
	};

	/// @return Mouse position relative to the top left of the screen.
	[[nodiscard]] V2_float GetMouseScreenPosition() const;

	/// Updates the user inputs and posts any triggered input events. Run internally when using game
	/// scenes.
	void Update(const EventSink& sink);
	void PollEvents(const EventSink& sink);

	/// @brief Number of keys stored in the SDL key states array.
	static constexpr std::size_t key_count_{ 512 };
	static constexpr std::size_t mouse_count_{ 3 };

	Window& window_;

	std::array<KeyState, key_count_> key_states_{};
	std::array<impl::Timestamp, key_count_> key_timestamps_{};
	std::array<MouseState, mouse_count_> mouse_states_{};
	std::array<impl::Timestamp, mouse_count_> mouse_timestamps_{};

	/// @brief Stored mouse positions are relative to the center of the window.
	V2_float mouse_position_;

	/// @brief Mouse position during the previous frame, relative to the center of the window.
	V2_float previous_mouse_position_;

	/// @brief Total scroll amount in the current frame (cumulative).
	V2_float mouse_scroll_delta_;

	/// @brief Scroll amount in the most recent scroll event.
	V2_float mouse_scroll_;

	/// @brief Timestamp of the most recent scroll event.
	impl::Timestamp mouse_scroll_timestamp_{ 0 };
};

PTGN_SERIALIZE_ENUM(
	InputHandler::MouseState, { { InputHandler::MouseState::Idle, "idle" },
								{ InputHandler::MouseState::Pressed, "pressed" },
								{ InputHandler::MouseState::Released, "released" } }
);

PTGN_SERIALIZE_ENUM(
	InputHandler::KeyState, { { InputHandler::KeyState::Idle, "idle" },
							  { InputHandler::KeyState::Pressed, "pressed" },
							  { InputHandler::KeyState::Released, "released" } }
);

} // namespace ptgn
