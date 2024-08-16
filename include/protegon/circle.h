#pragma once

#include "protegon/vector2.h"

namespace ptgn {

template <typename T = int>
struct Circle {
	Point<T> center{};
	T radius{ 0 };

	template <typename U>
	operator Circle<U>() const {
		return { static_cast<Point<U>>(center), static_cast<U>(radius) };
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
		return { static_cast<Point<U>>(center), static_cast<U>(radius), static_cast<U>(start_angle),
				 static_cast<U>(end_angle) };
	}
};

template <typename T = int>
struct Ellipse {
	Point<T> center{};
	Vector2<T> radius{};

	template <typename U>
	operator Circle<U>() const {
		return { static_cast<Point<U>>(center), static_cast<Vector2<T>>(radius) };
	}
};

} // namespace ptgn