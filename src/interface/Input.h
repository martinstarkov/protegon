#pragma once

#include "input/Mouse.h"
#include "input/Key.h"
#include "utility/Timer.h"
#include "math/Vector2.h"

namespace ptgn {

milliseconds GetMouseHeldTime(Mouse button);

namespace input {

V2_int GetMouseScreenPosition();

V2_int GetMouseAbsolutePosition();

bool MousePressed(Mouse button);

bool MouseReleased(Mouse button);

bool MouseDown(Mouse button);

bool MouseUp(Mouse button);

bool KeyPressed(Key key);

bool KeyReleased(Key key);

bool KeyDown(Key key);

bool KeyUp(Key key);

void Update();

/*
* @tparam Duration The unit of time measurement.
* @return True if the mouse button has been held for the given amount of time.
*/
template <typename Duration,
	tt::duration<Duration> = true>
inline bool MouseHeld(Mouse button, Duration time) {
	const auto held_time{ GetMouseHeldTime(button) };
	return held_time > time;
}

} // namespace input

} // namespace ptgn