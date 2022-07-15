#pragma once

#include "animation/Alignment.h"

namespace ptgn {

namespace animation {

struct Offset {
	Offset() = default;
	Offset(const V2_int& frame_size, const V2_int& reference_size, const Alignment& horizontal_alignment, const Alignment& vertical_alignment) : value{
			V2_double{ frame_size.x, reference_size.x }.DotProduct(alignment_vectors[static_cast<std::size_t>(horizontal_alignment)]),
			V2_double{ frame_size.y, reference_size.y }.DotProduct(alignment_vectors[static_cast<std::size_t>(vertical_alignment)])
	} {}
	operator V2_double() const {
		return value;
	}
	V2_double value;
};

} // namespace animation

} // namespace ptgn