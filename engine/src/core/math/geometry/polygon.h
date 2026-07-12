#pragma once

#include <concepts>
#include <iterator>
#include <ranges>
#include <vector>

#include "core/assert.h"
#include "core/math/math_utils.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "serialization/serialize.h"

namespace ptgn {

class Polygon {
public:
	std::vector<V2_float> vertices;

	constexpr Polygon() = default;

	template <std::ranges::input_range R>
		requires std::convertible_to<std::ranges::range_reference_t<R>, V2_float>
	constexpr explicit Polygon(const R& range) {
		vertices.assign(std::ranges::begin(range), std::ranges::end(range));
	}

	/// @return Centroid of the polygon.
	constexpr V2_float GetCenter() const {
		// Source: https://stackoverflow.com/a/63901131

		PTGN_ASSERT(vertices.size() >= 3);

		V2_float centroid;
		float signed_area{ 0.0f };
		V2_float v0{ 0.0f }; // Current verte
		V2_float v1{ 0.0f }; // Next vertex
		float a{ 0.0f };	 // Partial signed area

		const V2_float* prev{ &vertices.back() };
		const V2_float* next{ nullptr };

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

	constexpr std::vector<V2_float> GetWorldVertices(Transform transform) const {
		return transform.Apply(vertices);
	}

	/// @return True if all the interior angles are less than 180 degrees.
	constexpr bool IsConvex() const {
		auto count{ vertices.size() };

		PTGN_ASSERT(count >= 3, "Line or point convexity check is redundant");

		const auto get_cross = [](V2_float a, V2_float b, V2_float c) {
			return (b.x - a.x) * (c.y - b.y) - (b.y - a.y) * (c.x - b.x);
		};

		int sign{ static_cast<int>(Sign(get_cross(vertices[0], vertices[1], vertices[2]))) };

		// For convex polygons, all sequential point triplet cross products must have the same sign
		// (+ or -). For convex polygon every triplet makes turn in the same side (or CW, or CCW
		// depending on walk direction). For concave one some signs will differ (where inner angle
		// exceeds 180 degrees). Note that you don't need to calculate angle values. Source:
		// https://stackoverflow.com/a/40739079

		// Skip first point since that is the established reference.
		for (auto i{ 1uz }; i < count; ++i) {
			auto a{ vertices[i + 0] };
			auto b{ vertices[(i + 1) % count] };
			auto c{ vertices[(i + 2) % count] };

			auto new_sign{ static_cast<int>(Sign(get_cross(a, b, c))) };

			if (new_sign != sign) {
				// Polygon is concave.
				return false;
			}
		}

		// Convex.
		return true;
	}

	/// @return True if any of the interior angles are above 180 degrees.
	constexpr bool IsConcave() const {
		return !IsConvex();
	}

	constexpr bool operator==(const Polygon&) const = default;

	PTGN_REFLECT(Polygon, vertices)
};

} // namespace ptgn