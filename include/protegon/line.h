#pragma once

#include "vector2.h"
#include "color.h"

struct SDL_Renderer;

namespace ptgn {

namespace impl {

void DrawPixel(SDL_Renderer* renderer, int x, int y, const Color& color);
void DrawLine(int x1, int y1, int x2, int y2, const Color& color);
// Source: https://github.com/rtrussell/BBCSDL/blob/master/src/SDL2_gfxPrimitives.c
void DrawThickLine(int x, int y, int w, int h, const Color& color, std::uint8_t pixel_thickness);
void DrawCapsule(int x1, int y1, int x2, int y2, int r, const Color& color, bool draw_centerline);
void DrawArc(int x, int y, int arc_radius, float start_angle, float end_angle, const Color& color);
void DrawVerticalLineImpl(SDL_Renderer* renderer, int x, int y1, int y2);
void DrawVerticalLine(SDL_Renderer* renderer, int x, int y1, int y2, const Color& color);
void DrawHorizontalLineImpl(SDL_Renderer* renderer, int x1, int x2, int y);
void DrawHorizontalLine(SDL_Renderer* renderer, int x1, int x2, int y, const Color& color);
void DrawYPerpendicular(SDL_Renderer* B, int x1, int y1, int dx, int dy, int xstep, int ystep, int einit, int w_left, int w_right, int winit);
void DrawXPerpendicular(SDL_Renderer* B, int x1, int y1, int dx, int dy, int xstep, int ystep, int einit, int w_left, int w_right, int winit);
void DrawXThickLine(SDL_Renderer* B, int x1, int y1, int dx, int dy, int xstep, int ystep, double pixel_thickness, int pxstep, int pystep);
void DrawYThickLine(SDL_Renderer* B, int x1, int y1, int dx, int dy, int xstep, int ystep, double pixel_thickness, int pxstep, int pystep);
void DrawThickLineImpl(SDL_Renderer* B, int x1, int y1, int x2, int y2, double pixel_thickness);

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
	void Draw(const Color& color, std::uint8_t pixel_thickness = 1) const {
		V2_float scale = window::GetScale();
		if (pixel_thickness == 1)
			impl::DrawLine(static_cast<int>(a.x * scale.x),
						   static_cast<int>(a.y * scale.y),
						   static_cast<int>(b.x * scale.x),
						   static_cast<int>(b.y * scale.y),
						   color);
		else
			impl::DrawThickLine(static_cast<int>(a.x * scale.x),
						        static_cast<int>(a.y * scale.y),
						        static_cast<int>(b.x * scale.x),
						        static_cast<int>(b.y * scale.y),
						        color,
								pixel_thickness * scale.x);
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
		return { static_cast<Point<U>>(this->a),
				 static_cast<Point<U>>(this->b) };
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
		V2_float scale = window::GetScale();
		impl::DrawCapsule(static_cast<int>(segment.a.x * scale.x),
					      static_cast<int>(segment.a.y * scale.y),
					      static_cast<int>(segment.b.x * scale.x),
					      static_cast<int>(segment.b.y * scale.y),
						  static_cast<int>(r * scale.x),
					      color,
						  draw_centerline);
	}
};

} // namespace ptgn
