#pragma once

#include <array>
#include <cmath>
#include <cstdlib>
#include <vector>

#include "core/assert.h"
#include "core/math/math_utils.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "serialization/serialize.h"

namespace ptgn {

class Circle {
public:
	float radius{ 0.0f };

	constexpr Circle() = default;

	constexpr Circle(float radius) : radius{ radius } {} // NOSONAR

	/// @return Radius scaled relative to the transform.
	constexpr float GetRadius(Transform transform) const {
		auto avg_scale{ transform.GetAverageScale() };
		auto abs_scale{ std::abs(avg_scale) };
		return radius * abs_scale;
	}

	/// @return { radius * 2, radius * 2 }
	constexpr V2_float GetSize() const {
		return V2_float{ radius } * 2.0f;
	}

	/// @return { radius * 2, radius * 2 } scaled relative to the transform.
	constexpr V2_float GetSize(Transform transform) const {
		auto size{ GetSize() };
		auto abs_scale{ Abs(transform.scale) };
		return size * abs_scale;
	}

	constexpr std::array<V2_float, 4> GetWorldQuadVertices(Transform transform) const {
		auto vertices{ GetLocalQuadVertices() };
		return transform.Apply(vertices);
	}

	/// @brief Approximates a circle with a polygon of segment_count vertices. Must be above 3.
	constexpr std::vector<V2_float> GetVertices(Transform transform, std::size_t segment_count) {
		PTGN_ASSERT(segment_count > 3 && segment_count < 10000);

		std::vector<V2_float> vertices;
		vertices.reserve(segment_count);

		for (auto i{ 0uz }; i < segment_count; ++i) {
			auto t{ kTwoPi * static_cast<float>(i) / static_cast<float>(segment_count) };
			auto local{ V2_float{ std::cos(t), std::sin(t) } * radius };
			vertices.emplace_back(local);
		}

		return transform.Apply(vertices);
	}

	constexpr std::array<V2_float, 4> GetLocalQuadVertices() const {
		V2_float min{ -radius };
		V2_float max{ radius };
		return { min, V2_float{ max.x, min.y }, max, V2_float{ min.x, max.y } };
	}

	constexpr bool operator==(const Circle&) const = default;

	PTGN_REFLECT(Circle, radius)
};

} // namespace ptgn