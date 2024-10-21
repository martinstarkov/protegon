#pragma once

#include <vector>

#include "math/axis.h"
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

	// Rotation in radians.
	Polygon(const Rectangle<float>& rect, float rotation) {
		float c_a{ std::cos(rotation) };
		float s_a{ std::sin(rotation) };

		const auto rotated = [c_a, s_a](const V2_float& v) {
			return V2_float{ v.x * c_a - v.y * s_a, v.x * s_a + v.y * c_a };
		};

		vertices.resize(4);

		V2_float center{ rect.Center() };

		V2_float min{ rect.Min() - center };
		V2_float max{ rect.Max() - center };

		vertices[0] = rotated({ min.x, max.y }) + center;
		vertices[1] = rotated(max) + center;
		vertices[2] = rotated({ max.x, min.y }) + center;
		vertices[3] = rotated(min) + center;
	}

	explicit Polygon(const std::vector<V2_float>& vertices) : vertices{ vertices } {}

	// Source: https://stackoverflow.com/a/63901131
	[[nodiscard]] V2_float GetCentroid() const {
		V2_float centroid;
		float signed_area{ 0.0f };
		V2_float v0{ 0.0f }; // Current verte
		V2_float v1{ 0.0f }; // Next vertex
		float a{ 0.0f };	 // Partial signed area

		std::size_t lastdex	 = vertices.size() - 1;
		const V2_float* prev = &(vertices[lastdex]);
		const V2_float* next{ nullptr };

		// For all vertices in a loop
		for (const auto& vertex : vertices) {
			next		 = &vertex;
			v0			 = *prev;
			v1			 = *next;
			a			 = v0.Cross(v1);
			signed_area += a;
			centroid	+= (v0 + v1) * a;
			prev		 = next;
		}

		signed_area *= 0.5f;
		centroid	/= 6.0f * signed_area;

		return centroid;
	}

	[[nodiscard]] bool Overlaps(const Polygon& polygon) const {
		const auto overlap_axis = [](const Polygon& p1, const Polygon& p2,
									 const std::vector<Axis>& axes) {
			for (Axis a : axes) {
				auto [min1, max1] = impl::GetProjectionMinMax(p1, a);
				auto [min2, max2] = impl::GetProjectionMinMax(p2, a);

				if (!impl::IntervalsOverlap(min1, max1, min2, max2)) {
					return false;
				}
			}
			return true;
		};

		if (!overlap_axis(*this, polygon, impl::GetAxes(*this, false)) ||
			!overlap_axis(polygon, *this, impl::GetAxes(polygon, false))) {
			return false;
		}
	}

	// Works for both convex and concave polygons.
	[[nodiscard]] bool Overlaps(const V2_float& point) const {
		const auto& v{ vertices };
		std::size_t count{ v.size() };
		bool c{ false };
		std::size_t i{ 0 };
		std::size_t j{ count - 1 };
		// Algorithm from: https://wrfranklin.org/Research/Short_Notes/pnpoly.html
		for (; i < count; j = i++) {
			bool a{ (v[i].y > point.y) != (v[j].y > point.y) };
			bool b{ point.x < (v[j].x - v[i].x) * (point.y - v[i].y) / (v[j].y - v[i].y) + v[i].x };
			if (a && b) {
				c = !c;
			}
		}
		return c;
	}

	// @return True if internal polygon is entirely contained by this polygon.
	[[nodiscard]] bool Contains(const Polygon& internal) const {
		for (const auto& p : internal.vertices) {
			if (!Contains(p)) {
				return false;
			}
		}
		return true;
	}

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