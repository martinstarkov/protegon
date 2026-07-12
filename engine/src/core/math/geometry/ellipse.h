#pragma once

#include <array>

#include "core/assert.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/concepts.h"
#include "serialization/serialize.h"

namespace ptgn {

class Ellipse {
public:
	V2_float radius;

	constexpr Ellipse() = default;

	template <Arithmetic T>
	constexpr explicit Ellipse(T radius) : radius{ radius } {}

	template <Arithmetic T>
	constexpr explicit Ellipse(Vector2<T> radius) : radius{ radius } {}

	/// @return Radius scaled relative to the transform.
	constexpr V2_float GetRadius(Transform transform) const {
		auto abs_scale{ Abs(transform.scale) };
		return radius * abs_scale;
	}

	constexpr std::array<V2_float, 4> GetWorldQuadVertices(Transform transform) const {
		auto vertices{ GetLocalQuadVertices() };
		return transform.Apply(vertices);
	}

	constexpr std::array<V2_float, 4> GetLocalQuadVertices() const {
		auto min{ -radius };
		auto max{ radius };
		return { min, V2_float{ max.x, min.y }, max, V2_float{ min.x, max.y } };
	}

	constexpr bool operator==(const Ellipse&) const = default;

	PTGN_REFLECT(Ellipse, radius)
};

} // namespace ptgn