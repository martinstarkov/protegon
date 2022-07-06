#pragma once

#include "math/Vector2.h"

namespace ptgn {

namespace animation {

enum class Alignment : std::size_t {
	LEFT = 0,
	TOP = 0,
	RIGHT = 1,
	BOTTOM = 1,
	MIDDLE = 2,
};

static constexpr const V2_double alignment_vectors[3] = { { 0, 0 }, { 1, -1 }, { 0.5, -0.5 } };

} // namespace animation

} // namespace ptgn