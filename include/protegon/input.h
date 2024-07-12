#pragma once

#include "key.h"
#include "mouse.h"
#include "time.h"
#include "vector2.h"

// TODO: Consider adding scene camera relative position fetch?

namespace ptgn {

namespace input {

// Retrieves and updates latest user key strokes.
void Update();

void ForceUpdateMousePosition();

[[nodiscard]] V2_int GetMousePosition();

// @return The amount scrolled by the mouse vertically in the current frame,
// positive upward, negative downward. Zero if no scroll occurred.
[[nodiscard]] int MouseScroll();

[[nodiscard]] MouseState GetMouseState(Mouse button);

[[nodiscard]] bool MousePressed(Mouse button);

[[nodiscard]] bool MouseReleased(Mouse button);

[[nodiscard]] bool MouseDown(Mouse button);

[[nodiscard]] bool MouseUp(Mouse button);

[[nodiscard]] bool KeyPressed(Key key);

[[nodiscard]] bool KeyReleased(Key key);

[[nodiscard]] bool KeyDown(Key key);

[[nodiscard]] bool KeyUp(Key key);

[[nodiscard]] milliseconds GetMouseHeldTime(Mouse button);

/*
 * @tparam Duration The unit of time measurement.
 * @return True if the mouse button has been held for the given amount of time.
 */
template <typename Duration, type_traits::duration<Duration> = true>
[[nodiscard]] inline bool MouseHeld(Mouse button, Duration time) {
	const auto held_time{ GetMouseHeldTime(button) };
	return held_time > time;
}

} // namespace input

} // namespace ptgn