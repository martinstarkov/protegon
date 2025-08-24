#pragma once

#include <array>

#include "components/drawable.h"
#include "math/vector2.h"
#include "renderer/api/origin.h"
#include "serialization/serializable.h"

namespace ptgn {

struct Transform;
class Entity;

namespace impl {

class RenderData;

} // namespace impl

// Rect has no rotation center because this can be achieved via using a parent Entity and
// positioning it where the origin should be.

struct Rect {
	Rect() = default;

	Rect(const V2_float& min, const V2_float& max);
	explicit Rect(const V2_float& size);

	static void Draw(impl::RenderData& ctx, const Entity& entity);

	[[nodiscard]] V2_float GetSize() const;

	// @return Size scaled relative to the transform.
	[[nodiscard]] V2_float GetSize(const Transform& transform) const;

	// @return New transform offset by the draw_origin.
	[[nodiscard]] Transform Offset(const Transform& transform, Origin draw_origin) const;

	// @return Quad vertices relative to the transform where transform.position is taken as the
	// rectangle center.
	[[nodiscard]] std::array<V2_float, 4> GetWorldVertices(const Transform& transform) const;
	[[nodiscard]] std::array<V2_float, 4> GetLocalVertices() const;

	[[nodiscard]] std::array<V2_float, 4> GetWorldVertices(
		const Transform& transform, Origin draw_origin
	) const;

	// @return Center relative to the world.
	[[nodiscard]] V2_float GetCenter(const Transform& transform) const;

	V2_float min;
	V2_float max;

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(Rect, min, max)
};

PTGN_DRAWABLE_REGISTER(Rect);

} // namespace ptgn