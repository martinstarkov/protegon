#pragma once

#include <vector>

#include "protegon/vector2.h"
#include "renderer/origin.h"
#include "utility/debug.h"

namespace ptgn {

// Rectangles are axis aligned bounding boxes (AABBs).
template <typename T = float>
struct Rectangle {
	Vector2<T> pos;
	Vector2<T> size;
	Origin origin{ Origin::Center };

	Rectangle() = default;

	Rectangle(const Vector2<T>& pos, const Vector2<T>& size, Origin origin = Origin::Center) :
		pos{ pos }, size{ size }, origin{ origin } {}

	[[nodiscard]] Vector2<T> Half() const {
		return size / T{ 2 };
	}

	// @return Center position of rectangle.
	[[nodiscard]] Vector2<T> Center() const {
		return pos - GetOffsetFromCenter(size, origin);
	}

	// @return Bottom right position of rectangle.
	[[nodiscard]] Vector2<T> Max() const {
		return Center() + Half();
	}

	// @return Top left position of rectangle.
	[[nodiscard]] Vector2<T> Min() const {
		return Center() - Half();
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

	[[nodiscard]] bool IsZero() const {
		return pos.IsZero() && size.IsZero();
	}

	template <typename U>
	operator Rectangle<U>() const {
		return Rectangle<U>{ static_cast<Vector2<U>>(pos), static_cast<Vector2<U>>(size), origin };
	}
};

template <typename T = float>
struct RoundedRectangle : public Rectangle<T> {
	using Rectangle<T>::pos;
	using Rectangle<T>::size;

	RoundedRectangle() = default;

	RoundedRectangle(
		const Vector2<T>& pos, const Vector2<T>& size, T radius, Origin origin = Origin::Center
	) :
		Rectangle<T>{ pos, size, origin }, radius{ radius } {
		PTGN_ASSERT(
			radius < size.x / T{ 2 }, "Radius of rounded rectangle must be less than half its width"
		);
		PTGN_ASSERT(
			radius < size.y / T{ 2 },
			"Radius of rounded rectangle must be less than half its height"
		);
	}

	T radius{ 0 };
};

struct Polygon {
	Polygon() = default;

	explicit Polygon(const std::vector<V2_float>& vertices) : vertices{ vertices } {}

	std::vector<V2_float> vertices;
};

template <typename T = float>
struct Triangle {
	Triangle() = default;

	Triangle(const Vector2<T>& a, const Vector2<T>& b, const Vector2<T>& c) :
		a{ a }, b{ b }, c{ c } {}

	Vector2<T> a;
	Vector2<T> b;
	Vector2<T> c;

	template <typename U>
	operator Triangle<U>() const {
		return { static_cast<Triangle<U>>(a), static_cast<Triangle<U>>(b),
				 static_cast<Triangle<U>>(c) };
	}
};

} // namespace ptgn