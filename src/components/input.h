#pragma once

#include <vector>

#include "math/vector2.h"
#include "rendering/api/origin.h"
#include "serialization/serializable.h"

namespace ptgn {

struct Interactive {
	bool is_inside{ false };
	bool was_inside{ false };

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(Interactive, is_inside, was_inside)
};

struct Draggable {
	// Offset from the drag target center. Adding this value to the target position will maintain
	// the relative position between the mouse and drag target.
	V2_float offset;
	// Mouse position where the drag started.
	V2_float start;

	bool dragging{ false };

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(Draggable, offset, start, dragging)
};

struct InteractiveCircles {
	struct InteractiveCircle {
		float radius{ 0.0f };
		V2_float offset;

		friend bool operator==(const InteractiveCircle& a, const InteractiveCircle& b) {
			return NearlyEqual(a.radius, b.radius) && a.offset == b.offset;
		}

		friend bool operator!=(const InteractiveCircle& a, const InteractiveCircle& b) {
			return !(a == b);
		}

		PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(InteractiveCircle, radius, offset)
	};

	InteractiveCircles() = default;

	InteractiveCircles(float radius, const V2_float& offset = {}) :
		circles{ InteractiveCircle{ radius, offset } } {}

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(InteractiveCircles, circles)

	std::vector<InteractiveCircle> circles;
};

struct InteractiveRects {
	struct InteractiveRect {
		V2_float size;
		Origin origin{ Origin::Center };
		V2_float offset;

		friend bool operator==(const InteractiveRect& a, const InteractiveRect& b) {
			return a.size == b.size && a.origin == b.origin && a.offset == b.offset;
		}

		friend bool operator!=(const InteractiveRect& a, const InteractiveRect& b) {
			return !(a == b);
		}

		PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(InteractiveRect, size, origin, offset)
	};

	InteractiveRects() = default;

	InteractiveRects(
		const V2_float& size, Origin origin = Origin::Center, const V2_float& offset = {}
	) :
		rects{ InteractiveRect{ size, origin, offset } } {}

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(InteractiveRects, rects)

	std::vector<InteractiveRect> rects;
};

} // namespace ptgn