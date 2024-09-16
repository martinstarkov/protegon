#pragma once

#include "protegon/vector2.h"

namespace ptgn {

template <typename T = float>
struct Circle {
	Circle() = default;

	Circle(const Vector2<T>& center, T radius) : center{ center }, radius{ radius } {}

	Vector2<T> center;
	T radius{ 0 };

	template <typename U>
	operator Circle<U>() const {
		return { static_cast<Vector2<U>>(center), static_cast<U>(radius) };
	}
};

template <typename T = float>
struct Arc {
	Vector2<T> center;
	T radius{ 0 };
	// Radians counter-clockwise from the right.
	T start_angle{ 0 };
	// Radians counter-clockwise from the right.
	T end_angle{ 0 };

	template <typename U>
	operator Arc<U>() const {
		return { static_cast<Vector2<U>>(center), static_cast<U>(radius),
				 static_cast<U>(start_angle), static_cast<U>(end_angle) };
	}
};

template <typename T = float>
struct Ellipse {
	Vector2<T> center;
	Vector2<T> radius;

	template <typename U>
	operator Circle<U>() const {
		return { static_cast<Vector2<U>>(center), static_cast<Vector2<T>>(radius) };
	}
};

} // namespace ptgn