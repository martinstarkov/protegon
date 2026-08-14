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

		PTGN_ASSERT(
			!NearlyEqual(signed_area, 0.0f),
			"Cannot calculate centroid of a zero area polygon"
		);

		if (NearlyEqual(signed_area, 0.0f)) {
			return {};
		}

		centroid /= 6.0f * signed_area;

		return centroid;
	}

	constexpr std::vector<V2_float> GetWorldVertices(Transform transform) const {
		return transform.Apply(vertices);
	}

	/// @return True if the polygon is non degenerate and has no reflex vertices.
	/// Collinear consecutive vertices are allowed.
	constexpr bool IsConvex() const {
		const auto count{ vertices.size() };

		PTGN_ASSERT(count >= 3, "At least three vertices are required for a polygon");

		int winding_sign{ 0 };

		for (auto i{ 0uz }; i < count; ++i) {
			const auto& a{ vertices[i] };
			const auto& b{ vertices[(i + 1) % count] };
			const auto& c{ vertices[(i + 2) % count] };

			const float cross{ (b - a).Cross(c - b) };

			// A 180 degree turn does not make a polygon concave.
			if (NearlyEqual(cross, 0.0f)) {
				continue;
			}

			const int current_sign{ cross > 0.0f ? 1 : -1 };

			if (winding_sign == 0) {
				winding_sign = current_sign;
				continue;
			}

			if (current_sign != winding_sign) {
				return false;
			}
		}

		// If every cross product was zero, the polygon has zero area and
		// should not be considered a valid convex polygon.
		return winding_sign != 0;
	}

	/// @return True if the polygon contains at least one reflex vertex.
	/// Degenerate/entirely collinear polygons are not considered concave.
	constexpr bool IsConcave() const {
		const auto count{ vertices.size() };

		PTGN_ASSERT(count >= 3, "At least three vertices are required for a polygon");

		int winding_sign{ 0 };

		for (auto i{ 0uz }; i < count; ++i) {
			const auto& a{ vertices[i] };
			const auto& b{ vertices[(i + 1) % count] };
			const auto& c{ vertices[(i + 2) % count] };

			const float cross{ (b - a).Cross(c - b) };

			if (NearlyEqual(cross, 0.0f)) {
				continue;
			}

			const int current_sign{ cross > 0.0f ? 1 : -1 };

			if (winding_sign == 0) {
				winding_sign = current_sign;
				continue;
			}

			if (current_sign != winding_sign) {
				return true;
			}
		}

		return false;
	}

	constexpr bool operator==(const Polygon&) const = default;

	PTGN_REFLECT(Polygon, vertices)
};

} // namespace ptgn