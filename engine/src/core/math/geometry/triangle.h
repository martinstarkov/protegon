#pragma once

#include <array>

#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "serialization/serialize.h"

namespace ptgn {

class Triangle {
public:
	std::array<V2_float, 3> vertices;

	constexpr Triangle() = default;

	constexpr Triangle(V2_float a, V2_float b, V2_float c) : vertices{ a, b, c } {}

	constexpr explicit Triangle(const std::array<V2_float, 3>& vertices) : vertices{ vertices } {}

	constexpr std::array<V2_float, 3> GetWorldVertices(Transform transform) const {
		return transform.Apply(vertices);
	}

	constexpr bool operator==(const Triangle&) const = default;

	PTGN_SERIALIZE(Triangle, vertices)
};

} // namespace ptgn