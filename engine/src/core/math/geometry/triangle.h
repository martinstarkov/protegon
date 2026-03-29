#pragma once

#include <array>

#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "serialization/json/serialize.h"

namespace ptgn {

struct Triangle {
	Triangle() = default;

	Triangle(V2_float a, V2_float b, V2_float c);
	explicit Triangle(const std::array<V2_float, 3>& vertices);

	std::array<V2_float, 3> GetWorldVertices(Transform transform) const;

	std::array<V2_float, 3> GetLocalVertices() const;

	bool operator==(const Triangle&) const = default;

	V2_float a;
	V2_float b;
	V2_float c;

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(Triangle, a, b, c)
};

} // namespace ptgn