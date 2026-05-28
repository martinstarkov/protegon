#pragma once

#include <array>
#include <cstdlib>

#include "core/math/geometry/line.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "serialization/serialize.h"

namespace ptgn {

class Capsule {
public:
	Line line;
	float radius{ 0.0f };

	constexpr Capsule() = default;

	constexpr Capsule(V2_float start, V2_float end, float radius) :
		line{ start, end }, radius{ radius } {}

	/// @param out_size Optional parameter for the unrotated size of the quad.
	/// @return Quad vertices relative to the given transform for this line with a given a line
	/// width.
	constexpr std::array<V2_float, 4> GetWorldQuadVertices(
		Transform transform, V2_float* out_size = nullptr
	) const {
		return line.GetWorldQuadVertices(transform, 2.0f * radius, out_size);
	}

	/// @return Radius scaled relative to the transform.
	constexpr float GetRadius(Transform transform) const {
		auto avg_scale{ transform.GetAverageScale() };
		auto abs_scale{ std::abs(avg_scale) };
		return radius * abs_scale;
	}

	constexpr bool operator==(const Capsule&) const = default;

	PTGN_SERIALIZE(Capsule, line, radius)
};

} // namespace ptgn