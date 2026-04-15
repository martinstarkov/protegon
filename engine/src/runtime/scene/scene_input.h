#pragma once

#include "core/input/key.h"
#include "core/input/mouse.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "runtime/graphics/frame_context.h"

namespace ptgn {

class Scene;
class SceneContext;
class Window;

class SceneInput {
public:
	/// @return Mouse position relative to the specified viewport.
	V2_float GetMousePosition(Frame position_frame_of_reference = Frame::World) const;

	/// @return Mouse position relative to the specified viewport during the previous frame.
	V2_float GetPreviousMousePosition(Frame position_frame_of_reference = Frame::World) const;

	/// @return Mouse delta (current_position - previous_position) relative to the specified
	/// viewport.
	V2_float GetMouseDelta(Frame delta_frame_of_reference = Frame::World) const;

	/// @return The amount scrolled by the mouse in the current frame,
	/// positive upward right, negative downward left. Zero if no scroll occurred.
	V2_float GetMouseScroll() const;

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
	milliseconds GetMouseHeldTime(Mouse button) const;

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
	milliseconds GetKeyHeldTime(Key key) const;

private:
	friend class Scene;
	friend class SceneContext;

	explicit SceneInput(Scene& scene, const Window& window);

	/// @brief Convert position from being relative to the center of the window to being relative to
	/// the center of the specified viewport.
	V2_float GetMousePositionRelativeTo(V2_float position, Frame frame_of_reference) const;

	Scene& scene_;
	const Window& window_;
};

} // namespace ptgn