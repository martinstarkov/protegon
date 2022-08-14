#pragma once

#include "math/Vector.h"

namespace ptgn {

template <typename T = float>
using Point = math::Vector<T>;

template <typename T = float>
struct Line {
	Line() = default;
	Line(const math::Vector<T>& o, const math::Vector<T>& d) : origin{ o }, destination{ d } {}
	math::Vector<T> origin;
	math::Vector<T> destination;
	inline Line Resolve(const math::Vector<T>& p1, const math::Vector<T>& p2) const {
		return { origin - p1, destination - p2 };
	}
	inline math::Vector<T> Direction() const {
		return destination - origin;
	}
	template <typename U>
	operator Line<U>() const {
		return { static_cast<math::Vector<U>>(origin),
			     static_cast<math::Vector<U>>(destination) };
	}
};

template <typename T = float>
struct Ray : public Line<T> {
	Ray() = default;
	using Line::Line;
	inline Ray Resolve(const math::Vector<T>& p1, const math::Vector<T>& p2) const {
		return { origin - p1, destination - p2 };
	}
	template <typename U>
	operator Ray<U>() const {
		return { static_cast<math::Vector<U>>(origin),
				 static_cast<math::Vector<U>>(destination) };
	}
};

template <typename T = float>
struct Segment : public Line<T> {
	Segment() = default;
	using Line::Line;
	inline Segment Resolve(const math::Vector<T>& p1, const math::Vector<T>& p2) const {
		return { origin - p1, destination - p2 };
	}
	template <typename U>
	operator Segment<U>() const {
		return { static_cast<math::Vector<U>>(origin),
				 static_cast<math::Vector<U>>(destination) };
	}
};

template <typename T = float>
struct Capsule : public Segment<T> {
	using Segment::Segment;
	// TODO: Add explicit conversions to Line or Circle.
	Capsule() = default;
	Capsule(const Point<T>& a, const Point<T>& b, T r) : Segment{ a, b }, r{ r } {}
	T r{ 0 };
	inline T RadiusSquared() const {
		return r * r;
	}
	inline Capsule Resolve(const math::Vector<T>& p) const {
		return { a - p, b - p, r };
	}
	template <typename U>
	operator Capsule<U>() const {
		return { static_cast<Point<U>>(a),
				 static_cast<Point<U>>(b),
				 static_cast<U>(r) };
	}
};

template <typename T = float>
struct Circle {
	Circle() = default;
	Circle(const Point<T>& c, T r) : c{ c }, r{ r } {}
	Point<T> c;
	T r{ 0 };
	inline T RadiusSquared() const {
		return r * r;
	}
	inline Circle Resolve(const math::Vector<T>& p) const {
		return { c - p, r };
	}
	template <typename U>
	operator Circle<U>() const {
		return { static_cast<Point<U>>(c),
				 static_cast<U>(r) };
	}
};

template <typename T = float>
struct AABB {
	AABB() = default;
	AABB(const Point<T>& pos, const math::Vector<T>& size) : pos{ p }, size{ size } {}
	math::Vector<T> pos; // taken from top left
	math::Vector<T> size;
	inline AABB Resolve(const math::Vector<T>& p) const {
		return { pos - p, size };
	}
	inline math::Vector<T> Half() const {
		return size / 2;
	}
	inline Point<T> Center() const {
		return pos + Half();
	}
	inline Point<T> Max() const {
		return pos + size;
	}
	inline Point<T> Min() const {
		return pos;
	}
	template <typename U>
	operator AABB<U>() const {
		return { static_cast<Point<U>>(pos),
				 static_cast<math::Vector<U>>(size) };
	}
};

} // namespace ptgn