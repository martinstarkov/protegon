#pragma once

#include <array>

#include "components/drawable.h"
#include "math/vector2.h"
#include "serialization/serializable.h"

namespace ptgn {

struct Transform;
class Entity;

namespace impl {

class RenderData;

} // namespace impl

struct Line : public Drawable<Line> {
	Line() = default;

	Line(const V2_float& start, const V2_float& end);

	static void Draw(impl::RenderData& ctx, const Entity& entity);

	// @return Quad vertices relative to the given transform for this line with a given a line
	// width.
	[[nodiscard]] std::array<V2_float, 4> GetWorldQuadVertices(
		const Transform& transform, float line_width
	) const;

	[[nodiscard]] std::array<V2_float, 2> GetWorldVertices(const Transform& transform) const;

	[[nodiscard]] std::array<V2_float, 2> GetLocalVertices() const;

	V2_float start;
	V2_float end;

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(Line, start, end)
};

} // namespace ptgn