#pragma once

#include "vector2.h"
#include "color.h"

namespace ptgn {

namespace impl {

void DrawLine(int x1, int y1, int x2, int y2, const Color& color);
void DrawCapsule(int x1, int y1, int x2, int y2, int r, const Color& color, bool draw_centerline);
// Source: https://github.com/martinstarkov/SDL2_gfx/blob/master/SDL2_gfxPrimitives.c#L1183
void DrawArc(int x, int y, int arc_radius, float start_angle, float end_angle, const Color& color);

} // namespace impl

template <typename T = int>
struct Line {
	Point<T> a;
	Point<T> b;
	// p is the penetration vector.
	Line<T> Resolve(const Vector2<T>& p) const {
		return { a + p, b + p };
	}
	Vector2<T> Direction() const {
		return b - a;
	}
	template <typename U>
	operator Line<U>() const {
		return { static_cast<Point<U>>(a),
				 static_cast<Point<U>>(b) };
	}
	void Draw(const Color& color) const {
		impl::DrawLine(static_cast<int>(a.x),
					   static_cast<int>(a.y),
					   static_cast<int>(b.x),
					   static_cast<int>(b.y),
					   color);
	}
};

template <typename T = float>
struct Ray {
	Point<T> p;   // position
	Vector2<T> d; // direction (normalized)
	float t{};      // distance along d from position p to find endpoint of ray.
	template <typename U>
	operator Ray<U>() const {
		return { static_cast<Point<U>>(p),
				 static_cast<Vector2<U>>(d),
		         static_cast<U>(t) };
	}
};

template <typename T = float>
struct Segment : public Line<T> {
	template <typename U>
	operator Segment<U>() const {
		return { static_cast<Point<U>>(a),
				 static_cast<Point<U>>(b) };
	}
};

template <typename T = float>
struct Capsule {
	Segment<T> segment;
	T r{};
	template <typename U>
	operator Capsule<U>() const {
		return { static_cast<Segment<U>>(segment),
				 static_cast<U>(r) };
	}
	void Draw(const Color& color, bool draw_centerline = false) const {
		impl::DrawCapsule(static_cast<int>(segment.a.x),
					      static_cast<int>(segment.a.y),
					      static_cast<int>(segment.b.x),
					      static_cast<int>(segment.b.y),
						  static_cast<int>(r),
					      color,
						  draw_centerline);
	}
};

} // namespace ptgn