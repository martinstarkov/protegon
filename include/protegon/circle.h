#pragma once

#include "vector2.h"
#include "color.h"

namespace ptgn {

namespace impl {

// Source: https://rosettacode.org/wiki/Bitmap/Midpoint_circle_algorithm
// Source (used): https://www.geeksforgeeks.org/mid-point-circle-drawing-algorithm/
void DrawCircle(int x, int y, int r, const Color& color);

void DrawSolidCircle(int x, int y, int r, const Color& color);

} // namespace impl

template <typename T = float>
struct Circle {
	Point<T> center;
	T radius{ 0 };
	template <typename U>
	operator Circle<U>() const {
		return { static_cast<Point<U>>(center),
				 static_cast<U>(radius) };
	}
	void Draw(const Color& color) const {
		impl::DrawCircle(static_cast<int>(center.x),
						 static_cast<int>(center.y),
						 static_cast<int>(radius),
						 color);
	}
	void DrawSolid(const Color& color) const {
		impl::DrawSolidCircle(static_cast<int>(center.x),
						      static_cast<int>(center.y),
						      static_cast<int>(radius),
						      color);
	}
};

} // namespace ptgn