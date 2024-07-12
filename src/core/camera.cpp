#include "protegon/camera.h"

#include <functional>

#include "protegon/window.h"
#include "core/game.h"

namespace ptgn {

void CameraController::OnMouseMoveEvent([[maybe_unused]] const Event<MouseEvent>& e
) {
	/*static V2_int last	   = window::GetSize() / 2;
	static bool first_mouse = true;

	V2_int mouse_position = e.current;

	if (first_mouse) {
		last	   = mouse_position;
		first_mouse = false;
	}

	V2_int offset = mouse_position - last;

	last = mouse_position;*/

	static bool first_mouse = true;

	if (e.Type() == MouseEvent::Move) {
		const MouseMoveEvent& mouse = static_cast<const MouseMoveEvent&>(e);
		if (!first_mouse) {
			V2_float offset = mouse.current - mouse.previous;

			V2_float size = window::GetSize();

			V2_float scaled_offset = 2.0f * offset / size - V2_float{ 1.0f, 1.0f };

			Rotate(scaled_offset.x, scaled_offset.y, 0.0f);
		} else {
			first_mouse = false;
		}
	}
}

void CameraController::SubscribeToMouseEvents() {
	global::GetGame().event.mouse_event.Subscribe(
		(void*)this, std::bind(&CameraController::OnMouseMoveEvent, this, std::placeholders::_1)
	);
}

void CameraController::UnsubscribeFromMouseEvents() {
	global::GetGame().event.mouse_event.Unsubscribe((void*)this);
}


} // namespace ptgn
