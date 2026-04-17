#include "runtime/scene/scene_input.h"

#include "core/input/key.h"
#include "core/input/mouse.h"
#include "core/math/vector2.h"
#include "core/util/time.h"
#include "platform/window.h"
#include "runtime/graphics/frame_context.h"
#include "runtime/scene/scene.h"

namespace ptgn {

SceneInput::SceneInput(Scene& scene, const Window& window) : scene_{ scene }, window_{ window } {}

V2_float SceneInput::GetMousePosition(Frame position_frame_of_reference) const {
	auto position{ window_.GetMousePosition() };
	return GetMousePositionRelativeTo(position, position_frame_of_reference);
}

V2_float SceneInput::GetPreviousMousePosition(Frame position_frame_of_reference) const {
	return GetMousePositionRelativeTo(
		window_.GetPreviousMousePosition(), position_frame_of_reference
	);
}

V2_float SceneInput::GetMouseDelta(Frame delta_frame_of_reference) const {
	return GetMousePosition(delta_frame_of_reference) -
		   GetPreviousMousePosition(delta_frame_of_reference);
}

V2_float SceneInput::GetMouseScroll() const {
	return window_.GetMouseScroll();
}

bool SceneInput::MousePressed(Mouse button) const {
	return window_.MousePressed(button);
}

bool SceneInput::MouseReleased(Mouse button) const {
	return window_.MouseReleased(button);
}

bool SceneInput::MouseHeld(Mouse button) const {
	return window_.MouseHeld(button);
}

bool SceneInput::MouseHeld(Mouse button, milliseconds time) const {
	return window_.MouseHeld(button, time);
}

milliseconds SceneInput::GetMouseHeldTime(Mouse button) const {
	return window_.GetMouseHeldTime(button);
}

bool SceneInput::KeyPressed(Key key) const {
	return window_.KeyPressed(key);
}

bool SceneInput::KeyReleased(Key key) const {
	return window_.KeyReleased(key);
}

bool SceneInput::KeyHeld(Key key) const {
	return window_.KeyHeld(key);
}

milliseconds SceneInput::GetKeyHeldTime(Key key) const {
	return window_.GetKeyHeldTime(key);
}

V2_float SceneInput::GetMousePositionRelativeTo(
	V2_float position, Frame position_frame_of_reference
) const {
	return ConvertPoint(
		position, Frame::Window, position_frame_of_reference, FrameContext{ scene_ }
	);
}
} // namespace ptgn