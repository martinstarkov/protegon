#pragma once

#include <array>

#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "serialization/json/serialize.h"

namespace ptgn {

class Triangle {
public:
	constexpr Triangle() = default;

	constexpr Triangle(V2_float a, V2_float b, V2_float c) : vertices_{ a, b, c } {}

	constexpr explicit Triangle(const std::array<V2_float, 3>& vertices) : vertices_{ vertices } {}

	void SetVertices(V2_float a, V2_float b, V2_float c);

	std::array<V2_float, 3> GetLocalVertices() const;

	std::array<V2_float, 3> GetWorldVertices(Transform transform) const;

	std::array<V2_float, 4> GetWorldQuadVertices(Transform transform) const;

	bool operator==(const Triangle&) const = default;

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(Triangle, vertices_)

private:
	std::array<V2_float, 3> vertices_;
};

} // namespace ptgn