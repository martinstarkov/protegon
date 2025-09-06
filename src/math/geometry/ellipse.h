#pragma once

#include <array>

#include "components/drawable.h"
#include "math/vector2.h"
#include "serialization/serializable.h"

namespace ptgn {

class Entity;
struct Transform;

namespace impl {

class RenderData;

} // namespace impl

struct Ellipse {
	Ellipse() = default;

	explicit Ellipse(const V2_float& radius);

	static void Draw(impl::RenderData& ctx, const Entity& entity);

	// @return Center relative to the world.
	[[nodiscard]] V2_float GetCenter(const Transform& transform) const;

	[[nodiscard]] V2_float GetRadius() const;

	// @return Radius scaled relative to the transform.
	[[nodiscard]] V2_float GetRadius(const Transform& transform) const;

	[[nodiscard]] std::array<V2_float, 4> GetWorldQuadVertices(const Transform& transform) const;

	[[nodiscard]] std::array<V2_float, 4> GetLocalQuadVertices() const;

	V2_float radius;

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(Ellipse, radius)
};

PTGN_DRAWABLE_REGISTER(Ellipse);

} // namespace ptgn