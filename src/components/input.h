#pragma once

#include "components/generic.h"
#include "event/key.h"
#include "event/mouse.h"
#include "utility/utility.h"

namespace ptgn {

struct Interactive {};

struct Draggable {};

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

struct MouseOut : public CallbackComponent<void> {
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

// Dropzone events.

struct Drop : public CallbackComponent<void, V2_float> {
	using CallbackComponent::CallbackComponent;
};

struct DragEnter : public CallbackComponent<void> {
	using CallbackComponent::CallbackComponent;
};

struct DragOver : public CallbackComponent<void, V2_float> {
	using CallbackComponent::CallbackComponent;
};

struct DragLeave : public CallbackComponent<void> {
	using CallbackComponent::CallbackComponent;
};

} // namespace callback

} // namespace ptgn