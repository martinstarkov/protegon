#pragma once

#include <array> // std::array
#include <cstdlib> // std::size_t
#include <cstdint> // std::int64_t, etc
#include <tuple> // std::pair

#include "event/Inputs.h"
#include "math/Vector2.h"
#include "utility/Timer.h"

namespace ptgn {

namespace interfaces {

class InputHandler {
public:
	virtual V2_int GetMouseScreenPosition() const = 0;
	virtual V2_int GetMouseAbsolutePosition() const = 0;
	virtual bool MousePressed(Mouse button) const = 0;
	virtual bool MouseReleased(Mouse button) const = 0;
	virtual bool MouseDown(Mouse button) const = 0;
	virtual bool MouseUp(Mouse button) const = 0;
	virtual bool KeyPressed(Key key) const = 0;
	virtual bool KeyReleased(Key key) const = 0;
	virtual bool KeyDown(Key key) const = 0;
	virtual bool KeyUp(Key key) const = 0;
	virtual void Update() = 0;
};

} // namespace interfaces

namespace internal {

// Enum for storing states of mouse keys.
enum class MouseState {
	DOWN,
	PRESSED,
	UP,
	RELEASED
};

class SDLInputHandler : public interfaces::InputHandler {
public:
	// Update function called the beginning of each engine loop.
	virtual void Update() override;

	virtual V2_int GetMouseScreenPosition() const override;
	virtual V2_int GetMouseAbsolutePosition() const override;

	virtual bool MousePressed(Mouse button) const override;
	virtual bool MouseReleased(Mouse button) const override;
	virtual bool MouseDown(Mouse button) const override;
	virtual bool MouseUp(Mouse button) const override;

	virtual bool KeyPressed(Key key) const override;
	virtual bool KeyReleased(Key key) const override;
	virtual bool KeyDown(Key key) const override;
	virtual bool KeyUp(Key key) const override;
	
	/*
	* @tparam Duration The unit of time measurement.
	* @return True if the mouse button has been held for the given amount of time.
	*/
	template <typename Duration, 
	    type_traits::is_duration_e<Duration> = true>
	bool MouseHeld(Mouse button, Duration time) {
	    auto [state, timer] = GetMouseStateAndTimer(button);
	    // Retrieve held time in nanoseconds for maximum precision.
	    const auto held_time{ timer->Elapsed<nanoseconds>() };
	    // Comparison units handled by chrono.
	    return held_time > time;
	}
	
private:
	// Number of keys stored in the SDL key states array. For creating previous key states array.
	static const std::size_t KEY_COUNT{ 512 };

	/*
	* @param button Mouse enum corresponding to the desired button.
	* @return Current state of the given mouse button.
	*/
	MouseState GetMouseState(Mouse button) const;

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

SDLInputHandler& GetSDLInputHandler();

} // namespace internal

namespace services {

interfaces::InputHandler& GetInputHandler();
	
} // namespace services

} // namespace ptgn