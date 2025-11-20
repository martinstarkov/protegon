#pragma once

#include <array>

#include "ecs/components/origin.h"
#include "math/vector2.h"
#include "serialization/json/serialize.h"

namespace ptgn {

struct Transform;

// RoundedRect has no rotation center because this can be achieved via using a parent Entity and
// positioning it where the origin should be.
struct RoundedRect {
	RoundedRect() = default;

	RoundedRect(V2_float min, V2_float max, float radius);
	RoundedRect(V2_float size, float radius);

	[[nodiscard]] V2_float GetSize() const;
	[[nodiscard]] float GetRadius() const;

	// @return Size scaled relative to the transform.
	[[nodiscard]] V2_float GetSize(const Transform& transform) const;
	[[nodiscard]] float GetRadius(const Transform& transform) const;

	// @return New transform offset by the draw_origin.
	[[nodiscard]] Transform Offset(const Transform& transform, Origin draw_origin) const;

	// @return Quad vertices relative to the transform where transform.position is taken as the
	// rounded rectangle center.
	[[nodiscard]] std::array<V2_float, 4> GetWorldQuadVertices(const Transform& transform) const;
	[[nodiscard]] std::array<V2_float, 4> GetLocalQuadVertices() const;

	[[nodiscard]] std::array<V2_float, 4> GetWorldQuadVertices(
		const Transform& transform, Origin draw_origin
	) const;

	// @return Center relative to the world.
	[[nodiscard]] V2_float GetCenter(const Transform& transform) const;

	bool operator==(const RoundedRect&) const = default;

	V2_float min;
	V2_float max;
	float radius{ 0.0f };

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(RoundedRect, min, max, radius)
};

} // namespace ptgn