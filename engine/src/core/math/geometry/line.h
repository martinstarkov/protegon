#pragma once

#include <array>

#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "serialization/json/serialize.h"

namespace ptgn {

struct Line {
	Line() = default;

	Line(V2_float start, V2_float end);

	/// @param out_size Optional parameter for the unrotated size of the quad.
	/// @return Quad vertices relative to the given transform for this line with a given a line
	/// width.
	std::array<V2_float, 4> GetWorldQuadVertices(
		Transform transform, float line_width = 1.0f, V2_float* out_size = nullptr
	) const;

	std::array<V2_float, 2> GetWorldVertices(Transform transform) const;

	std::array<V2_float, 2> GetLocalVertices() const;

	bool operator==(const Line&) const = default;

	V2_float start;
	V2_float end;

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(Line, start, end)
};

} // namespace ptgn