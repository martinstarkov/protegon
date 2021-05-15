#pragma once

#include <array> // std::array
#include <cstdlib> // std::size_t
#include <cstdint> // std::int64_t, etc

#include "event/Inputs.h"

#include "math/Vector2.h"

#include "utils/Timer.h"

// TODO: Make mouse update only be within the window you are looking in.

namespace engine {

class Engine;

class InputHandler {
public:
	/*
	* @return The x and y positions of the mouse relative to the top left of the focused window.
	*/
	static V2_int GetMousePosition();
	/*
	* @return True if mouse button is pressed, false otherwise.
	*/
	static bool MousePressed(MouseButton button);
	/*
	* @return True if mouse button is released, false otherwise.
	*/
	static bool MouseReleased(MouseButton button);
	/*
	* @return True the first cycle a mouse button is pressed, false otherwise.
	*/
	static bool MouseDown(MouseButton button);
	/*
	* @return True the first cycle a mouse button is released, false otherwise.
	*/
	static bool MouseUp(MouseButton button);
	/*
	* @return True if the mouse button has been held for the given amount of seconds.
	*/
	static bool MouseHeldFor(MouseButton button, std::int64_t milliseconds = DEFAULT_MOUSE_HELD_TIME);

	/*
	* @return True if given key is pressed, false otherwise.
	*/
	static bool KeyPressed(Key key);
	/*
	* @return True if given key is released, false otherwise.
	*/
	static bool KeyReleased(Key key);
	/*
	* @return True the first cycle a key is pressed, false otherwise.
	*/
	static bool KeyDown(Key key);
	/*
	* @return True the first cycle a key is released, false otherwise.
	*/
	static bool KeyUp(Key key);
private:
	// The amount of time for a mouse button press to be considered held.
	// Unit: seconds.
	static constexpr std::int64_t DEFAULT_MOUSE_HELD_TIME{ 250 };

	// Number of keys stored in the SDL key states array. For creating previous key states array.
	static constexpr std::size_t KEY_COUNT{ 512 };

	friend class Engine;

	static void Init();
	static InputHandler* instance_;
	static InputHandler& GetInstance();

	static void Update();

	void UpdateKeyStates();
	void UpdatePreviousMouseStates();
	void UpdateMouseStates();

	std::int64_t GetMousePressedTime(MouseButton button);
	bool GetPreviousMouseState(MouseButton button);

	V2_int mouse_position_;

	Timer left_mouse_pressed_time_;
	Timer right_mouse_pressed_time_;
	Timer middle_mouse_pressed_time_;

	bool left_was_pressed_{ false };
	bool right_was_pressed_{ false };
	bool middle_was_pressed_{ false };
	
	std::array<std::uint8_t, KEY_COUNT> previous_key_states_{};
};

} // namespace engine