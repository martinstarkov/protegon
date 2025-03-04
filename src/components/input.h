#pragma once

#include <vector>

#include "components/generic.h"
#include "ecs/ecs.h"
#include "event/key.h"
#include "event/mouse.h"
#include "math/geometry/circle.h"
#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "renderer/origin.h"

namespace ptgn {

struct Interactive {
	bool is_inside{ false };
	bool was_inside{ false };
};

struct Draggable {
	// Offset from the drag target center. Adding this value to the target position will maintain
	// the relative position between the mouse and drag target.
	V2_float offset;
	// Mouse position where the drag started.
	V2_float start;
	// Drag target.
	ecs::Entity target;
	bool dragging{ false };
};

struct InteractiveCircles {
	struct InteractiveCircle {
		Circle circle;
		V2_float offset;
	};

	InteractiveCircles() = default;

	InteractiveCircles(float radius, const V2_float& offset) :
		circles{ InteractiveCircle{ Circle{ radius }, offset } } {}

	std::vector<InteractiveCircle> circles;
};

struct InteractiveRects {
	struct InteractiveRect {
		Rect rect;
		V2_float offset;
	};

	InteractiveRects() = default;

	InteractiveRects(const V2_float& size, Origin origin, const V2_float& offset) :
		rects{ InteractiveRect{ Rect{ size, origin }, offset } } {}

	std::vector<InteractiveRect> rects;
};

namespace callback {

// Key events

struct KeyDown : public CallbackComponent<Key> {
	using CallbackComponent::CallbackComponent;
};

struct KeyPressed : public CallbackComponent<Key> {
	using CallbackComponent::CallbackComponent;
};

struct KeyUp : public CallbackComponent<Key> {
	using CallbackComponent::CallbackComponent;
};

// Mouse events.

struct MouseDown : public CallbackComponent<Mouse> {
	using CallbackComponent::CallbackComponent;
};

struct MouseDownOutside : public CallbackComponent<Mouse> {
	using CallbackComponent::CallbackComponent;
};

struct MouseMove : public CallbackComponent<V2_float> {
	using CallbackComponent::CallbackComponent;
};

struct MouseEnter : public CallbackComponent<V2_float> {
	using CallbackComponent::CallbackComponent;
};

struct MouseLeave : public CallbackComponent<V2_float> {
	using CallbackComponent::CallbackComponent;
};

struct MouseOut : public CallbackComponent<V2_float> {
	using CallbackComponent::CallbackComponent;
};

struct MouseOver : public CallbackComponent<V2_float> {
	using CallbackComponent::CallbackComponent;
};

struct MouseUp : public CallbackComponent<Mouse> {
	using CallbackComponent::CallbackComponent;
};

struct MouseUpOutside : public CallbackComponent<Mouse> {
	using CallbackComponent::CallbackComponent;
};

struct MousePressed : public CallbackComponent<Mouse> {
	using CallbackComponent::CallbackComponent;
};

// Scroll amount in each direction.
struct MouseScroll : public CallbackComponent<V2_int> {
	using CallbackComponent::CallbackComponent;
};

// Draggable events.

struct DragStart : public CallbackComponent<V2_float> {
	using CallbackComponent::CallbackComponent;
};

struct DragStop : public CallbackComponent<V2_float> {
	using CallbackComponent::CallbackComponent;
};

struct Drag : public CallbackComponent<V2_float> {
	using CallbackComponent::CallbackComponent;
};

struct DragEnter : public CallbackComponent<V2_float> {
	using CallbackComponent::CallbackComponent;
};

struct DragLeave : public CallbackComponent<V2_float> {
	using CallbackComponent::CallbackComponent;
};

struct DragOver : public CallbackComponent<V2_float> {
	using CallbackComponent::CallbackComponent;
};

struct DragOut : public CallbackComponent<V2_float> {
	using CallbackComponent::CallbackComponent;
};

// Dropzone events.

// struct Drop : public CallbackComponent<V2_float> {
//	using CallbackComponent::CallbackComponent;
// };

} // namespace callback

} // namespace ptgn