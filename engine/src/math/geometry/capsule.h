#pragma once

#include <array>

#include "math/vector2.h"
#include "serialization/json/serialize.h"

namespace ptgn {

struct Transform;

struct Capsule {
	Capsule() = default;

	Capsule(V2_float start, V2_float end, float radius);

	// @param out_size Optional parameter for the unrotated size of the quad.
	// @return Quad vertices relative to the given transform for this line with a given a line
	// width.
	[[nodiscard]] std::array<V2_float, 4> GetWorldQuadVertices(
		const Transform& transform, V2_float* out_size = nullptr
	) const;

	[[nodiscard]] std::array<V2_float, 2> GetWorldVertices(const Transform& transform) const;

	[[nodiscard]] std::array<V2_float, 2> GetLocalVertices() const;

	[[nodiscard]] float GetRadius() const;

	// @return Radius scaled relative to the transform.
	[[nodiscard]] float GetRadius(const Transform& transform) const;

	bool operator==(const Capsule&) const = default;

	V2_float start;
	V2_float end;
	float radius{ 0.0f };

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(Capsule, start, end, radius)
};

} // namespace ptgn