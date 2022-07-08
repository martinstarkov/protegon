#pragma once

#include "animation/Animation.h"
#include "animation/Alignment.h"

namespace ptgn {

namespace animation {

struct Offset {
	Offset() = default;
	Offset(const Animation& animation, const V2_int& reference_size, const Alignment& horizontal_alignment, const Alignment& vertical_alignment) : value{
			math::Floor(V2_double{ animation.frame_size.x, reference_size.x }.DotProduct(alignment_vectors[static_cast<std::size_t>(horizontal_alignment)])),
			math::Floor(V2_double{ animation.frame_size.y, reference_size.y }.DotProduct(alignment_vectors[static_cast<std::size_t>(vertical_alignment)]))
	} {}
	V2_int value;
};

} // namespace animation

} // namespace ptgn