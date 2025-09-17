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

struct Line {
	Line() = default;

	Line(const V2_float& start, const V2_float& end);

	static void Draw(const Entity& entity);

	// @param out_size Optional parameter for the unrotated size of the quad.
	// @return Quad vertices relative to the given transform for this line with a given a line
	// width.
	[[nodiscard]] std::array<V2_float, 4> GetWorldQuadVertices(
		const Transform& transform, float line_width = 1.0f, V2_float* out_size = nullptr
	) const;

	[[nodiscard]] std::array<V2_float, 2> GetWorldVertices(const Transform& transform) const;

	[[nodiscard]] std::array<V2_float, 2> GetLocalVertices() const;

	bool operator==(const Line&) const = default;

	V2_float start;
	V2_float end;

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(Line, start, end)
};

PTGN_DRAWABLE_REGISTER(Line);

} // namespace ptgn