#pragma once

#include "protegon/vector2.h"

namespace ptgn {

template <typename T = float>
struct Line {
	Line() = default;

	Line(const Point<T>& a, const Point<T>& b) : a{ a }, b{ b } {}

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
		return { static_cast<Point<U>>(a), static_cast<Point<U>>(b) };
	}
};

template <typename T = float>
struct Segment : public Line<T> {
	using Line<T>::Line;

	template <typename U>
	operator Segment<U>() const {
		return { static_cast<Point<U>>(this->a), static_cast<Point<U>>(this->b) };
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
		return { static_cast<Line<U>>(segment), static_cast<U>(radius) };
	}
};

} // namespace ptgn
