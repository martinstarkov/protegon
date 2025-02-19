#pragma once

#include "components/draw.h"
#include "components/generic.h"
#include "event/key.h"
#include "event/mouse.h"
#include "utility/utility.h"

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

struct InteractiveRadius : public Radius {
	using Radius::Radius;
};

struct InteractiveSize : public Size {
	using Size::Size;
};

namespace callback {

// Key events

struct KeyDown : public CallbackComponent<void, ptgn::Key> {
	using CallbackComponent::CallbackComponent;
};

struct KeyPressed : public CallbackComponent<void, ptgn::Key> {
	using CallbackComponent::CallbackComponent;
};

struct KeyUp : public CallbackComponent<void, ptgn::Key> {
	using CallbackComponent::CallbackComponent;
};

// Mouse events.

struct MouseDown : public CallbackComponent<void, ptgn::Mouse> {
	using CallbackComponent::CallbackComponent;
};

struct MouseDownOutside : public CallbackComponent<void, ptgn::Mouse> {
	using CallbackComponent::CallbackComponent;
};

struct MouseMove : public CallbackComponent<void, V2_float> {
	using CallbackComponent::CallbackComponent;
};

struct MouseEnter : public CallbackComponent<void, V2_float> {
	using CallbackComponent::CallbackComponent;
};

struct MouseLeave : public CallbackComponent<void, V2_float> {
	using CallbackComponent::CallbackComponent;
};

struct MouseOut : public CallbackComponent<void, V2_float> {
	using CallbackComponent::CallbackComponent;
};

struct MouseOver : public CallbackComponent<void, V2_float> {
	using CallbackComponent::CallbackComponent;
};

struct MouseUp : public CallbackComponent<void, ptgn::Mouse> {
	using CallbackComponent::CallbackComponent;
};

struct MouseUpOutside : public CallbackComponent<void, ptgn::Mouse> {
	using CallbackComponent::CallbackComponent;
};

struct MousePressed : public CallbackComponent<void, ptgn::Mouse> {
	using CallbackComponent::CallbackComponent;
};

// Scroll amount in each direction.
struct MouseScroll : public CallbackComponent<void, V2_int> {
	using CallbackComponent::CallbackComponent;
};

// Draggable events.

struct DragStart : public CallbackComponent<void, V2_float> {
	using CallbackComponent::CallbackComponent;
};

struct DragStop : public CallbackComponent<void, V2_float> {
	using CallbackComponent::CallbackComponent;
};

struct Drag : public CallbackComponent<void, V2_float> {
	using CallbackComponent::CallbackComponent;
};

struct DragEnter : public CallbackComponent<void, V2_float> {
	using CallbackComponent::CallbackComponent;
};

struct DragLeave : public CallbackComponent<void, V2_float> {
	using CallbackComponent::CallbackComponent;
};

struct DragOver : public CallbackComponent<void, V2_float> {
	using CallbackComponent::CallbackComponent;
};

struct DragOut : public CallbackComponent<void, V2_float> {
	using CallbackComponent::CallbackComponent;
};

// Dropzone events.

// struct Drop : public CallbackComponent<void, V2_float> {
//	using CallbackComponent::CallbackComponent;
// };

} // namespace callback

} // namespace ptgn