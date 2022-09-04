#pragma once

#include "vector2.h"

namespace ptgn {

// Rectangles are axis aligned bounding boxes (AABBs).
template <typename T = float>
struct Rectangle {
	Point<T> position; // Point taken from top left.
	Vector2<T> size; // Full width and height.
	Vector2<T> Half() const {
		return size / 2;
	}
	Point<T> Center() const {
		return position + Half();
	}
	Point<T> Max() const {
		return position + size;
	}
	Point<T> Min() const {
		return position;
	}
	template <typename U>
	operator Rectangle<U>() const {
		return { static_cast<Point<U>>(position),
				 static_cast<Vector2<U>>(size) };
	}
};

} // namespace ptgn