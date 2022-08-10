#pragma once

#include <cstdlib> // std::size_t

#include "math/Vector2.h"

namespace ptgn {

namespace animation {

enum class Alignment : std::size_t {
	LEFT   = 0,
	TOP    = 0,
	RIGHT  = 1,
	BOTTOM = 1,
	MIDDLE = 2,
	CENTER = 2
};

inline constexpr V2_float alignment_vectors[3]{ { 0, 0 }, { 1, -1 }, { 0.5f, -0.5f } };

} // namespace animation

} // namespace ptgn