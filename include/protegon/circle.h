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
		if (pixel_thickness == 1)
			impl::DrawCircle(static_cast<int>(c.x),
							 static_cast<int>(c.y),
							 static_cast<int>(r),
							 color);
		else
			impl::DrawThickCircle(static_cast<int>(c.x),
							      static_cast<int>(c.y),
							      static_cast<int>(r),
							      color,
								  pixel_thickness);
	}
	void DrawSolid(const Color& color) const {
		impl::DrawSolidCircle(static_cast<int>(c.x),
						      static_cast<int>(c.y),
						      static_cast<int>(r),
						      color);
	}
};

} // namespace ptgn