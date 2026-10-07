#include "runtime/scene/scene_input.h"

#include "core/assert.h"
#include "core/input/key.h"
#include "core/input/mouse.h"
#include "core/math/vector2.h"
#include "core/util/time.h"
#include "platform/window.h"
#include "runtime/graphics/frame_context.h"
#include "runtime/scene/scene.h"

namespace ptgn {

SceneInput::SceneInput(const Window& window, const SceneInputState& state, Scene& scene) :
	scene_{ &scene }, window_{ window }, state_{ state } {}

bool SceneInput::IsEnabled() const {
	return state_.enabled;
}

V2_float SceneInput::GetMousePosition(Frame position_frame_of_reference) const {
	if (!IsEnabled()) {
		return {};
	}

	auto position{ window_.GetMousePosition() };
	if (position_frame_of_reference == Frame::Window) {
		return position;
	}
	return GetMousePositionRelativeTo(position, position_frame_of_reference);
}

V2_float SceneInput::GetPreviousMousePosition(Frame position_frame_of_reference) const {
	if (!IsEnabled()) {
		return {};
	}

	auto prev_position{ window_.GetPreviousMousePosition() };
	if (position_frame_of_reference == Frame::Window) {
		return prev_position;
	}
	return GetMousePositionRelativeTo(prev_position, position_frame_of_reference);
}

V2_float SceneInput::GetMouseDelta(Frame delta_frame_of_reference) const {
	if (!IsEnabled()) {
		return {};
	}

	return GetMousePosition(delta_frame_of_reference) -
		   GetPreviousMousePosition(delta_frame_of_reference);
}

V2_float SceneInput::GetMouseScroll() const {
	return IsEnabled() ? window_.GetMouseScroll() : V2_float{};
}

bool SceneInput::MousePressed(Mouse button) const {
	return IsEnabled() && window_.MousePressed(button);
}

bool SceneInput::MouseReleased(Mouse button) const {
	return IsEnabled() && window_.MouseReleased(button);
}

bool SceneInput::MouseHeld(Mouse button) const {
	return IsEnabled() && window_.MouseHeld(button);
}

bool SceneInput::MouseHeld(Mouse button, milliseconds time) const {
	return IsEnabled() && window_.MouseHeld(button, time);
}

milliseconds SceneInput::GetMouseHeldTime(Mouse button) const {
	return IsEnabled() ? window_.GetMouseHeldTime(button) : milliseconds{};
}

bool SceneInput::KeyPressed(Key key) const {
	return IsEnabled() && window_.KeyPressed(key);
}

bool SceneInput::KeyReleased(Key key) const {
	return IsEnabled() && window_.KeyReleased(key);
}

bool SceneInput::KeyHeld(Key key) const {
	return IsEnabled() && window_.KeyHeld(key);
}

bool SceneInput::KeyHeld(Key key, milliseconds time) const {
	return IsEnabled() && window_.KeyHeld(key, time);
}

milliseconds SceneInput::GetKeyHeldTime(Key key) const {
	return IsEnabled() ? window_.GetKeyHeldTime(key) : milliseconds{};
}

V2_float SceneInput::GetMousePositionRelativeTo(
	V2_float position, Frame position_frame_of_reference
) const {
	PTGN_ASSERT(scene_);
	return ConvertPoint(
		position, Frame::Window, position_frame_of_reference, FrameContext{ *scene_ }
	);
}

void SceneInput::Rebind(Scene& scene) {
	scene_ = &scene;
}

} // namespace ptgn
