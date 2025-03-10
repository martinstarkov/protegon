#pragma once

#include <array>

#include "math/vector2.h"

namespace ptgn {

struct Line {
	Line() = default;
	Line(const V2_float& start, const V2_float& end);

	[[nodiscard]] std::array<V2_float, 4> GetQuadVertices(float line_width) const;

	V2_float start;
	V2_float end;
};

struct Capsule {
	Capsule() = default;
	Capsule(const V2_float& start, const V2_float& end, float radius);

	V2_float start;
	V2_float end;
	float radius{ 0.0f };
};

} // namespace ptgn