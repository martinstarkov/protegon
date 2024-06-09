#pragma once

#include "vector2.h"
#include "color.h"

struct SDL_Renderer;

namespace ptgn {

namespace impl {

std::shared_ptr<SDL_Renderer> SetDrawMode(const Color& color);

void               DrawPoint(int x,  int y,  const Color& color);
void                DrawLine(int x1, int y1, int x2, int y2, const Color& color);
void           DrawThickLine(int x1, int y1, int x2, int y2, double pixel_thickness, const Color& color);
void             DrawCapsule(int x1, int y1, int x2, int y2, int r, const Color& color);
void        DrawSolidCapsule(int x1, int y1, int x2, int y2, int r, const Color& color);
void        DrawThickCapsule(int x1, int y1, int x2, int y2, int r, double pixel_thickness, const Color& color);
void        DrawVerticalLine(int x,  int y1, int y2, const Color& color);
void   DrawThickVerticalLine(int x,  int y1, int y2, double pixel_thickness, const Color& color);
void      DrawHorizontalLine(int x1, int x2, int y,  const Color& color);
void DrawThickHorizontalLine(int x1, int x2, int y,  double pixel_thickness, const Color& color);

// Taken from: https://github.com/rtrussell/BBCSDL/blob/master/src/SDL2_gfxPrimitives.c (with some modifications)
void               DrawPointImpl(SDL_Renderer* renderer, int x,  int y);
void                DrawLineImpl(SDL_Renderer* renderer, int x1, int y1, int x2, int y2);
void           DrawThickLineImpl(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, double pixel_thickness);
void             DrawCapsuleImpl(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, int r);
void        DrawSolidCapsuleImpl(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, int r);
void        DrawThickCapsuleImpl(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, int r, double pixel_thickness);
void        DrawVerticalLineImpl(SDL_Renderer* renderer, int x,  int y1, int y2);
void   DrawThickVerticalLineImpl(SDL_Renderer* renderer, int x,  int y1, int y2, double pixel_thickness);
void      DrawHorizontalLineImpl(SDL_Renderer* renderer, int x1, int x2, int y);
void DrawThickHorizontalLineImpl(SDL_Renderer* renderer, int x1, int x2, int y, double pixel_thickness);
void          DrawXPerpendicular(SDL_Renderer* renderer, int x1, int y1, int dx, int dy, int xstep, int ystep, int einit, int w_left,  int w_right, int winit);
void          DrawYPerpendicular(SDL_Renderer* renderer, int x1, int y1, int dx, int dy, int xstep, int ystep, int einit, int w_left,  int w_right, int winit);
void              DrawXThickLine(SDL_Renderer* renderer, int x1, int y1, int dx, int dy, int xstep, int ystep, double pixel_thickness, int pxstep,  int pystep);
void              DrawYThickLine(SDL_Renderer* renderer, int x1, int y1, int dx, int dy, int xstep, int ystep, double pixel_thickness, int pxstep,  int pystep);

} // namespace impl

template <typename T = float>
struct Line {
	Line() = default;
	Line(const Point<T> a, Point<T> b) : a{ a }, b{ b } {}

	Point<T> a{};
	Point<T> b{};

	// p is the penetration vector.
	[[nodiscard]] Line<T> Resolve(const Vector2<T>& p) const {
		return { a + p, b + p };
	}
	[[nodiscard]] Vector2<T> Direction() const {
		return b - a;
	}

	template <typename U>
	operator Line<U>() const {
		return {
			static_cast<Point<U>>(a),
			static_cast<Point<U>>(b)
		};
	}

	void Draw(const Color& color, double pixel_thickness = 1) const {
		if (pixel_thickness <= 1) {
			impl::DrawLine(
				static_cast<int>(a.x),
				static_cast<int>(a.y),
				static_cast<int>(b.x),
				static_cast<int>(b.y),
				color
			);
		} else {
			impl::DrawThickLine(
				static_cast<int>(a.x),
				static_cast<int>(a.y),
				static_cast<int>(b.x),
				static_cast<int>(b.y),
				pixel_thickness,
				color
			);
		}
	}
};

template <typename T = float>
struct Segment : public Line<T> {
	using Line<T>::Line;
	template <typename U>
	operator Segment<U>() const {
		return {
			static_cast<Point<U>>(this->a),
			static_cast<Point<U>>(this->b)
		};
	}
};

template <typename T = float>
struct Capsule {
	Capsule() = default;
	Capsule(const Line<T>& segment, T radius) : segment{ segment }, radius{ radius } {}

	Line<T> segment;
	T radius{};

	template <typename U>
	operator Capsule<U>() const {
		return {
			static_cast<Line<U>>(segment),
			static_cast<U>(radius)
		};
	}

	void Draw(const Color& color, double pixel_thickness = 1) const {
		if (pixel_thickness <= 1) {
			impl::DrawCapsule(
				static_cast<int>(segment.a.x),
				static_cast<int>(segment.a.y),
				static_cast<int>(segment.b.x),
				static_cast<int>(segment.b.y),
				static_cast<int>(radius),
				color
			);
		} else {
			impl::DrawThickCapsule(
				static_cast<int>(segment.a.x),
				static_cast<int>(segment.a.y),
				static_cast<int>(segment.b.x),
				static_cast<int>(segment.b.y),
				static_cast<int>(radius),
				pixel_thickness,
				color
			);
		}
	}

	void DrawSolid(const Color& color) const {
		impl::DrawSolidCapsule(
			static_cast<int>(segment.a.x),
			static_cast<int>(segment.a.y),
			static_cast<int>(segment.b.x),
			static_cast<int>(segment.b.y),
			static_cast<int>(radius),
			color
		);
	}
};

} // namespace ptgn
