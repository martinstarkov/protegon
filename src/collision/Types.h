#pragma once

#include "math/Vector2.h"

namespace ptgn {

template <typename T>
struct Point {
	Point(const math::Vector2<T>& p) : p{ p } {}
	math::Vector2<T> p;
	template <typename T>
	operator math::Vector2<T>() const {
		return p;
	}
	template <typename U>
	operator Point<U>() const {
		return { static_cast<math::Vector2<U>>(p) };
	}
};

template <typename T>
struct Line {
	Line(const math::Vector2<T>& o, const math::Vector2<T>& d) : origin{ o }, destination{ d } {}
	math::Vector2<T> origin;
	math::Vector2<T> destination;
	inline math::Vector2<T> Direction() const {
		return destination - origin;
	}
	template <typename U>
	operator Line<U>() const {
		return { static_cast<math::Vector2<U>>(origin),
			     static_cast<math::Vector2<U>>(destination) };
	}
};

template <typename T>
struct Ray : public Line<T> {
	using Line::Line;
};

template <typename T>
struct Segment : public Line<T> {
	using Line::Line;
};

template <typename T>
struct Capsule : public Segment<T> {
	using Segment::Segment;
	// TODO: Add explicit conversions to Line or Circle.
	Capsule(const math::Vector2<T>& o, const math::Vector2<T>& d, T r) : Segment{ o, d }, radius{ r } {}
	T radius{ 0 };
};

template <typename T>
struct Circle {
	Circle(const math::Vector2<T>& c, T r) : center{ c }, radius{ r } {}
	math::Vector2<T> center;
	T radius{ 0 };
	inline T RadiusSquared() const {
		return radius * radius;
	}
};

template <typename T>
struct AABB {
	AABB(const math::Vector2<T>& p, const math::Vector2<T>& s) : position{ p }, size{ s } {}
	math::Vector2<T> position; // taken from top left
	math::Vector2<T> size;
	inline math::Vector2<T> Center() const {
		return position + size / T{ 2 };
	}
	inline math::Vector2<T> Max() const {
		return position + size;
	}
	inline math::Vector2<T> Min() const {
		return position;
	}
	template <typename U>
	operator AABB<U>() const {
		return { static_cast<math::Vector2<U>>(position),
				 static_cast<math::Vector2<U>>(size) };
	}
};

} // namespace ptgn