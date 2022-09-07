#pragma once

#include "vector2.h"
#include "color.h"

namespace ptgn {

namespace impl {

void DrawRectangle(int x, int y, int w, int h, const Color& color);

void DrawSolidRectangle(int x, int y, int w, int h, const Color& color);

} // namespace impl

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
	void Draw(const Color& color) const {
		impl::DrawRectangle(static_cast<int>(position.x), 
							static_cast<int>(position.y),
							static_cast<int>(size.x),
							static_cast<int>(size.y), 
							color);
	}
	void DrawSolid(const Color& color) const {
		impl::DrawSolidRectangle(static_cast<int>(position.x),
								 static_cast<int>(position.y),
								 static_cast<int>(size.x),
								 static_cast<int>(size.y),
								 color);
	}
};

} // namespace ptgn