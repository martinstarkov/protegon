#pragma once

#include "math/Vector2.h"

namespace ptgn {

template <typename T = float>
using Point = Vector2<T>;

template <typename T = float>
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
};

template <typename T = float>
struct Ray : public Line<T> {
	template <typename U>
	operator Ray<U>() const {
		return { static_cast<Point<U>>(a),
				 static_cast<Point<U>>(b) };
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
struct Capsule : public Segment<T> {
	T r{ 0 };
	Capsule<T> Resolve(const Vector2<T>& p) const {
		return { a + p, b + p, r };
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
	Point<T> c;
	T r{ 0 };
	Circle<T> Resolve(const Vector2<T>& p) const {
		return { c + p, r };
	}
	template <typename U>
	operator Circle<U>() const {
		return { static_cast<Point<U>>(c),
				 static_cast<U>(r) };
	}
};

template <typename T = float>
struct AABB {
	Point<T> p; // point taken from top left
	Vector2<T> s; // size is full width and height
	AABB<T> Resolve(const Vector2<T>& pen) const {
		return { p + pen, s };
	}
	Vector2<T> Half() const {
		return s / 2;
	}
	Point<T> Center() const {
		return p + Half();
	}
	Point<T> Max() const {
		return p + s;
	}
	Point<T> Min() const {
		return p;
	}
	template <typename U>
	operator AABB<U>() const {
		return { static_cast<Point<U>>(p),
			     static_cast<Vector2<U>>(s) };
	}
};

} // namespace ptgn