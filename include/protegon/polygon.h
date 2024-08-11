#pragma once

#include <vector>

#include "protegon/vector2.h"

namespace ptgn {

// Rectangles are axis aligned bounding boxes (AABBs).
template <typename T = float>
struct Rectangle {
	// Position taken from top left.
	Point<T> pos;
	Vector2<T> size;
	Rectangle() = default;

	Rectangle(const Point<T>& pos, const Vector2<T>& size) : pos{ pos }, size{ size } {}

	[[nodiscard]] Vector2<T> Half() const {
		return size / T{ 2 };
	}

	[[nodiscard]] Point<T> Center() const {
		return pos + Half();
	}

	[[nodiscard]] Point<T> Max() const {
		return pos + size;
	}

	[[nodiscard]] Point<T> Min() const {
		return pos;
	}

	[[nodiscard]] Rectangle<T> Offset(
		const Vector2<T>& pos_amount, const Vector2<T>& size_amount = {}
	) const {
		return { pos + pos_amount, size + size_amount };
	}

	template <typename U>
	[[nodiscard]] Rectangle<T> Scale(const Vector2<U>& scale) const {
		return { pos * scale, size * scale };
	}

	template <typename U>
	[[nodiscard]] Rectangle<T> ScalePos(const Vector2<U>& pos_scale) const {
		return { pos * pos_scale, size };
	}

	template <typename U>
	[[nodiscard]] Rectangle<T> ScaleSize(const Vector2<U>& size_scale) const {
		return { pos, size * size_scale };
	}

	[[nodiscard]] bool IsZero() const {
		return pos.IsZero() && size.IsZero();
	}

	template <typename U>
	operator Rectangle<U>() const {
		return Rectangle<U>{ static_cast<Point<U>>(pos), static_cast<Vector2<U>>(size) };
	}
};

template <typename T = float>
struct RoundedRectangle : public Rectangle<T> {
	using Rectangle<T>::pos;
	using Rectangle<T>::size;

	RoundedRectangle() = default;

	RoundedRectangle(const Point<T>& pos, const Vector2<T>& size, T radius) :
		Rectangle<T>{ pos, size }, radius{ radius } {}

	T radius{ 0 };
};

struct Polygon {
	Polygon() = default;

	Polygon(const std::vector<V2_float>& vertices) : vertices{ vertices } {}

	std::vector<V2_float> vertices;
};

template <typename T = float>
struct Triangle {
	Triangle() = default;

	Triangle(const Point<T>& a, const Point<T>& b, const Point<T>& c) : a{ a }, b{ b }, c{ c } {}

	Point<T> a{};
	Point<T> b{};
	Point<T> c{};

	template <typename U>
	operator Triangle<U>() const {
		return { static_cast<Triangle<U>>(a), static_cast<Triangle<U>>(b),
				 static_cast<Triangle<U>>(c) };
	}
};

} // namespace ptgn