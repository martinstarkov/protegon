#pragma once

#include <functional>

#include "vector2.h"
#include "color.h"

struct SDL_Renderer;

namespace ptgn {

namespace impl {

void DrawCircle(int x, int y, int r, const Color& color);
void DrawSolidCircle(int x, int y, int r, const Color& color);
void DrawSolidCircleSliced(int x, int y, int r, const Color& color, std::function<bool(double y_frac)> condition);
void DrawThickCircle(int x, int y, int r, double pixel_thickness, const Color& color);

void DrawEllipse(int x, int y, int rx, int ry, const Color& color);
void DrawSolidEllipse(int x, int y, int rx, int ry, const Color& color);
void DrawThickEllipse(int x, int y, int rx, int ry, double pixel_thickness, const Color& color);

// Angles are degrees from east (right) direction (clockwise positive)
void DrawArc(int x, int y, int arc_radius, double start_angle, double end_angle, const Color& color);
// Angles are degrees from east (right) direction (clockwise positive)
void DrawSolidArc(int x, int y, int arc_radius, double start_angle, double end_angle, const Color& color);
// Angles are degrees from east (right) direction (clockwise positive)
void DrawThickArc(int x, int y, int arc_radius, double start_angle, double end_angle, double pixel_thickness, const Color& color);

// Sources: 
// https://rosettacode.org/wiki/Bitmap/Midpoint_circle_algorithm
// https://www.geeksforgeeks.org/mid-point-circle-drawing-algorithm/
// Taken from: https://github.com/rtrussell/BBCSDL/blob/master/src/SDL2_gfxPrimitives.c (with some modifications)
void DrawCircleImpl(SDL_Renderer* renderer, int x, int y, int r);
void DrawSolidCircleImpl(SDL_Renderer* renderer, int x, int y, int r);
void DrawSolidCircleSlicedImpl(SDL_Renderer* renderer, int x, int y, int r, std::function<bool(double y_frac)> condition);
void DrawThickCircleImpl(SDL_Renderer* renderer, int x, int y, int r, double pixel_thickness);
void DrawEllipseImpl(SDL_Renderer* renderer, int x, int y, int rx, int ry);
void DrawSolidEllipseImpl(SDL_Renderer* renderer, int x, int y, int rx, int ry);
void DrawThickEllipseImpl(SDL_Renderer* renderer, int x, int y, int rx, int ry, double pixel_thickness);
void DrawArcImpl(SDL_Renderer* renderer, int x, int y, int arc_radius, double start_angle, double end_angle);
void DrawSolidArcImpl(SDL_Renderer* renderer, int x, int y, int arc_radius, double start_angle, double end_angle);
void DrawThickArcImpl(SDL_Renderer* renderer, int x, int y, int arc_radius, double start_angle, double end_angle, double pixel_thickness);

} // namespace impl

template <typename T = int>
struct Circle {
	Point<T> center{};
	T radius{ 0 };
	template <typename U>
	operator Circle<U>() const {
		return {
			static_cast<Point<U>>(center),
			static_cast<U>(radius)
		};
	}
	void Draw(const Color& color, double pixel_thickness = 1) const {
		if (pixel_thickness <= 1) {
			impl::DrawCircle(
				static_cast<int>(center.x),
				static_cast<int>(center.y),
				static_cast<int>(radius),
				color
			);
		} else {
			impl::DrawThickCircle(
				static_cast<int>(center.x),
				static_cast<int>(center.y),
				static_cast<int>(radius),
				pixel_thickness,
				color
			);
		}
	}
	void DrawSolid(const Color& color) const {
		impl::DrawSolidCircle(
			static_cast<int>(center.x),
			static_cast<int>(center.y),
			static_cast<int>(radius),
			color
		);
	}
	// condition should be a lambda which returns true for pixels where the circle is rendered.
	// y_frac indicates which fraction of the radius the circle is rendering: 
	// y_frac is 0.0 (top of circle) to 1.0 (bottom of circle), 0.5 is the center.
	void DrawSolidSliced(const Color& color, std::function<bool(double y_frac)> condition) const {
		impl::DrawSolidCircleSliced(
			static_cast<int>(center.x),
			static_cast<int>(center.y),
			static_cast<int>(radius),
			color,
			condition
		);
	}
};

template <typename T = float>
struct Arc {
	Point<T> center{};
	T radius{ 0 };
	// Degrees from east (right) direction (clockwise positive)
	T start_angle{ 0 };
	// Degrees from east (right) direction (clockwise positive)
	T end_angle{ 0 };
	template <typename U>
	operator Arc<U>() const {
		return {
			static_cast<Point<U>>(center),
			static_cast<U>(radius),
			static_cast<U>(start_angle),
			static_cast<U>(end_angle)
		};
	}
	void Draw(const Color& color, double pixel_thickness = 1) const {
		if (pixel_thickness <= 1) {
			impl::DrawArc(
				static_cast<int>(center.x),
				static_cast<int>(center.y),
				static_cast<int>(radius),
				static_cast<double>(start_angle),
				static_cast<double>(end_angle),
				color
			);
		} else {
			impl::DrawThickArc(
				static_cast<int>(center.x),
				static_cast<int>(center.y),
				static_cast<int>(radius),
				static_cast<double>(start_angle),
				static_cast<double>(end_angle),
				pixel_thickness,
				color
			);
		}
	}
	void DrawSolid(const Color& color) const {
		impl::DrawSolidArc(
			static_cast<int>(center.x),
			static_cast<int>(center.y),
			static_cast<int>(radius),
			static_cast<double>(start_angle),
			static_cast<double>(end_angle),
			color
		);
	}
};

template <typename T = int>
struct Ellipse {
	Point<T> center{};
	Vector2<T> radius{};
	template <typename U>
	operator Circle<U>() const {
		return {
			static_cast<Point<U>>(center),
			static_cast<Vector2<T>>(radius)
		};
	}
	void Draw(const Color& color, double pixel_thickness = 1) const {
		if (pixel_thickness <= 1) {
			impl::DrawEllipse(
				static_cast<int>(center.x),
				static_cast<int>(center.y),
				static_cast<int>(radius.x),
				static_cast<int>(radius.y),
				color
			);
		} else {
			impl::DrawThickEllipse(
				static_cast<int>(center.x),
				static_cast<int>(center.y),
				static_cast<int>(radius.x),
				static_cast<int>(radius.y),
				pixel_thickness,
				color
			);
		}
	}
	void DrawSolid(const Color& color) const {
		impl::DrawSolidEllipse(
			static_cast<int>(center.x),
			static_cast<int>(center.y),
			static_cast<int>(radius.x),
			static_cast<int>(radius.y),
			color
		);
	}
};

} // namespace ptgn