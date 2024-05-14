#pragma once

#include "vector2.h"
#include "color.h"

struct SDL_Renderer;

namespace ptgn {

namespace impl {

// Source: https://rosettacode.org/wiki/Bitmap/Midpoint_circle_algorithm
// Source (used): https://www.geeksforgeeks.org/mid-point-circle-drawing-algorithm/
void DrawCircle(int x, int y, int r, const Color& color);
void DrawThickCircle(int x, int y, int r, const Color& color, std::uint8_t pixel_thickness);
void DrawSolidCircleSliced(int x, int y, int r, const Color& color, std::function<bool(float y_frac)> condition);
void DrawSolidCircle(int x, int y, int r, const Color& color);
// Source: https://github.com/rtrussell/BBCSDL/blob/master/src/SDL2_gfxPrimitives.c
void DrawEllipse(SDL_Renderer* renderer, int x, int y, int rx, int ry, const Color& color);
void DrawThickEllipse(SDL_Renderer* renderer, int xc, int yc, int xr, int yr, const Color& color, std::uint8_t pixel_thickness);

} // namespace impl

template <typename T = int>
struct Circle {
	Point<T> c;
	T r{ 0 };
	template <typename U>
	operator Circle<U>() const {
		return { static_cast<Point<U>>(c),
				 static_cast<U>(r) };
	}
	void Draw(const Color& color, std::uint8_t pixel_thickness = 1) const {
		V2_float scale = window::GetScale();
		if (pixel_thickness == 1)
			impl::DrawCircle(static_cast<int>(c.x * scale.x),
							 static_cast<int>(c.y * scale.y),
							 static_cast<int>(r * scale.x),
							 color);
		else
			impl::DrawThickCircle(static_cast<int>(c.x * scale.x),
							      static_cast<int>(c.y * scale.y),
							      static_cast<int>(r * scale.x),
							      color,
								  pixel_thickness * scale.x);
	}
	void DrawSolid(const Color& color) const {
		V2_float scale = window::GetScale();
		impl::DrawSolidCircle(static_cast<int>(c.x * scale.x),
						      static_cast<int>(c.y * scale.y),
						      static_cast<int>(r * scale.x),
						      color);
	}
	// condition is a lambda which returns true for pixels where the circle is rendered.
	// y_frac indicates which fraction of the radius the circle is rendering: 
	// y_frac is 0.0 (top of circle) to 1.0 (bottom of circle), 0.5 is the center.
	void DrawSolidSliced(const Color& color, std::function<bool(float y_frac)> condition) const {
		V2_float scale = window::GetScale();
		impl::DrawSolidCircleSliced(static_cast<int>(c.x * scale.x),
			static_cast<int>(c.y * scale.y),
			static_cast<int>(r * scale.x),
			color,
			condition);
	}
};

} // namespace ptgn