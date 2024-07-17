#include "camera.h"

#include "core/window.h"
#include "protegon/game.h"

namespace ptgn {

void CameraController::OnMouseMoveEvent([[maybe_unused]] const MouseMoveEvent& e) {
	/*static V2_int last	   = game.window.GetSize() / 2;
	static bool first_mouse = true;

	V2_int mouse_position = e.current;

	if (first_mouse) {
		last	   = mouse_position;
		first_mouse = false;
	}

	V2_int offset = mouse_position - last;

	last = mouse_position;*/

	static bool first_mouse = true;

	if (game.input.MousePressed(Mouse::Left)) {
		const MouseMoveEvent& mouse = static_cast<const MouseMoveEvent&>(e);
		if (!first_mouse) {
			V2_float offset = mouse.current - mouse.previous;

			V2_float size = game.window.GetSize();

			V2_float scaled_offset = offset / size;

			// OpenGL y-axis is flipped compared to SDL mouse position.
			Rotate(scaled_offset.x, -scaled_offset.y, 0.0f);
		} else {
			first_mouse = false;
		}
	}
}

void CameraController::SubscribeToMouseEvents() {
	game.event.mouse.Subscribe(MouseEvent::Move, (void*)this, std::function([&](const MouseMoveEvent& e) {
		OnMouseMoveEvent(e);
	}));
}

void CameraController::UnsubscribeFromMouseEvents() {
	game.event.mouse.Unsubscribe((void*)this);
}


} // namespace ptgn
