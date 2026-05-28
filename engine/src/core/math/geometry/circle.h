#pragma once

#include <array>
#include <cstdlib>

#include "core/assert.h"
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

	constexpr std::array<V2_float, 4> GetLocalQuadVertices() const {
		V2_float min{ -radius };
		V2_float max{ radius };
		PTGN_ASSERT(min != max, "Cannot get local vertices for a circle with size zero");
		return { min, V2_float{ max.x, min.y }, max, V2_float{ min.x, max.y } };
	}

	constexpr bool operator==(const Circle&) const = default;

	PTGN_SERIALIZE(Circle, radius)
};

} // namespace ptgn