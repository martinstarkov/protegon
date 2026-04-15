#pragma once

#include "core/input/mouse.h"
#include "core/math/vector2.h"

namespace ptgn::event {

struct MouseEnter {};

struct MouseLeave {};

struct MouseMoveOver {};

struct MousePressedOver {
	operator Mouse() const { // NOSONAR
		return button;
	}

	Mouse button;
};

struct MouseHeldOver {
	operator Mouse() const { // NOSONAR
		return button;
	}

	Mouse button;
};

struct MouseReleasedOver {
	operator Mouse() const { // NOSONAR
		return button;
	}

	Mouse button;
};

struct MouseScrollOver {
	operator V2_float() const { // NOSONAR
		return scroll_delta;
	}

	V2_float scroll_delta;
};

struct MouseMoveOut {};

struct MousePressedOut {
	operator Mouse() const { // NOSONAR
		return button;
	}

	Mouse button;
};

struct MouseHeldOut {
	operator Mouse() const { // NOSONAR
		return button;
	}

	Mouse button;
};

struct MouseReleasedOut {
	operator Mouse() const { // NOSONAR
		return button;
	}

	Mouse button;
};

struct MouseScrollOut {
	operator V2_float() const { // NOSONAR
		return scroll_delta;
	}

	V2_float scroll_delta;
};

} // namespace ptgn::event