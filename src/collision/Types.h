#pragma once

#include "math/Vector2.h"

namespace ptgn {

template <typename T>
struct Point {
	math::Vector2<T> p; // point
	template <typename T>
	operator math::Vector2<T>() const {
		return p;
	}
};

template <typename T>
struct Line {
	math::Vector2<T> a; // origin
	math::Vector2<T> b; // destination
	inline math::Vector2<T> Direction() const {
		return b - a;
	}
};

template <typename T>
struct Ray : public Line<T> {};

template <typename T>
struct Segment : public Line<T> {};

template <typename T>
struct Capsule : public Segment<T> {
	T r{ 0 }; // radius
};

template <typename T>
struct Circle {
	math::Vector2<T> c; // center
	T r{ 0 }; // radius
};

template <typename T>
struct AABB {
	math::Vector2<T> p; // top left position
	math::Vector2<T> s; // size
	inline math::Vector2<T> Center() const {
		return p + s / T{ 2 };
	}
};

} // namespace ptgn