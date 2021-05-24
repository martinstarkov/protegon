#pragma once

#include <array> // std::array
#include <cstdlib> // std::size_t
#include <cstdint> // std::int64_t, etc
#include <tuple> // std::pair

#include "event/Inputs.h"
#include "math/Vector2.h"
#include "utils/Timer.h"
#include "utils/Singleton.h"

namespace engine {

class InputHandler : public Singleton<InputHandler> {
public:

	/*
	* @return The x and y positions of the mouse relative to the top left of the focused window.
	*/
	static V2_int GetMousePosition();

	/*
	* @return True if mouse button is pressed, false otherwise.
	*/
	static bool MousePressed(Mouse button);

	/*
	* @return True if mouse button is released, false otherwise.
	*/
	static bool MouseReleased(Mouse button);

	/*
	* @return True the first cycle a mouse button is pressed, false otherwise.
	*/
	static bool MouseDown(Mouse button);

	/*
	* @return True the first cycle a mouse button is released, false otherwise.
	*/
	static bool MouseUp(Mouse button);

	/*
	* @tparam Duration The unit of time measurement.
	* @return True if the mouse button has been held for the given amount of time.
	*/
	template <typename Duration, 
		type_traits::is_duration_e<Duration> = true>
	static bool MouseHeld(Mouse button, Duration time) {
		auto& instance{ GetInstance() };
		auto [state, timer] = instance.GetMouseStateAndTimer(button);
		// Retrieve held time in nanoseconds for maximum precision.
		const auto held_time{ timer->Elapsed<nanoseconds>() };
		// Comparison units handled by chrono.
		return held_time > time;
	}

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
	friend class Engine;
	friend class Singleton<InputHandler>;

	// Enum for storing states of mouse keys.
	enum class MouseState {
		DOWN,
		PRESSED,
		UP,
		RELEASED
	};

	// Number of keys stored in the SDL key states array. For creating previous key states array.
	static constexpr std::size_t KEY_COUNT{ 512 };

	/*
	* @param button Mouse enum corresponding to the desired button.
	* @return Current state of the given mouse button.
	*/
	static MouseState GetMouseState(Mouse button);

	// Update function called the beginning of each engine loop.
	static void Update();

	InputHandler() = default;

	// Updates previous key states for key up and down check.
	void UpdateKeyStates();

	// Updates previous mouse states for mouse up and down check.
	void UpdateMouseState(Mouse button);

	/*
	* @param button Mouse enum corresponding to the desired button.
	* @return Pair of pointers to the mouse state and timer for a given button, 
	* pair of nullptrs if no such button exists.
	*/
	std::pair<MouseState*, Timer*> GetMouseStateAndTimer(Mouse button);

	// Mouse position (updated upon GetMousePosition call).

	V2_int mouse_position_;

	// Mouse states.

	MouseState left_mouse_{ MouseState::RELEASED };
	MouseState right_mouse_{ MouseState::RELEASED };
	MouseState middle_mouse_{ MouseState::RELEASED };

	// Mouse button held for timers.

	Timer left_mouse_timer_;
	Timer right_mouse_timer_;
	Timer middle_mouse_timer_;

	// Previous loop cycle key states for comparison with current.
	std::array<std::uint8_t, KEY_COUNT> previous_key_states_{};
};

} // namespace engine