#pragma once

#include "vector2.h"
#include "color.h"

struct SDL_Renderer;

namespace ptgn {

namespace impl {

void DrawRectangle(int x, int y, int w, int h, const Color& color);
// Source: https://github.com/rtrussell/BBCSDL/blob/master/src/SDL2_gfxPrimitives.c
void DrawThickRectangle(int x, int y, int w, int h, const Color& color, std::uint8_t pixel_thickness);
void DrawSolidRectangle(int x, int y, int w, int h, const Color& color);
void DrawSolidRectangleImpl(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, const Color& color);

} // namespace impl

// Rectangles are axis aligned bounding boxes (AABBs).
template <typename T = int>
struct Rectangle {
	Point<T> pos;    // Point taken from top left.
	Vector2<T> size; // Full width and height.
	Vector2<T> Half() const {
		return size / 2;
	}
	Point<T> Center() const {
		return pos + Half();
	}
	Point<T> Max() const {
		return pos + size;
	}
	Point<T> Min() const {
		return pos;
	}
	Rectangle<T> Offset(const Vector2<T>& pos_amount, const Vector2<T>& size_amount = {}) const {
		return { pos + pos_amount, size + size_amount };
	}
	template <typename U>
	operator Rectangle<U>() const {
		return { static_cast<Point<U>>(pos),
				 static_cast<Vector2<U>>(size) };
	}
	void Draw(const Color& color, std::uint8_t pixel_thickness = 1) const {
		if (pixel_thickness == 1)
			impl::DrawRectangle(static_cast<int>(pos.x),
								static_cast<int>(pos.y),
								static_cast<int>(size.x),
								static_cast<int>(size.y), 
								color);
		else
			impl::DrawThickRectangle(static_cast<int>(pos.x),
								     static_cast<int>(pos.y),
								     static_cast<int>(size.x),
								     static_cast<int>(size.y),
								     color,
									 pixel_thickness);
	}
	void DrawSolid(const Color& color) const {
		impl::DrawSolidRectangle(static_cast<int>(pos.x),
								 static_cast<int>(pos.y),
								 static_cast<int>(size.x),
								 static_cast<int>(size.y),
								 color);
	}
};

} // namespace ptgn