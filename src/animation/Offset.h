#pragma once

#include "animation/Animation.h"
#include "animation/Alignment.h"

namespace ptgn {

namespace animation {

struct Offset {
	Offset() = default;
	Offset(const Animation& animation, const V2_int& reference_size, const Alignment& horizontal_alignment, const Alignment& vertical_alignment) {
		V2_double horizontal_size_vector{ animation.size.x, reference_size.x };
		V2_double vertical_size_vector{ animation.size.y, reference_size.y };
		V2_double horizontal_alignment_vector{ alignment_vectors[static_cast<std::size_t>(horizontal_alignment)] };
		V2_double vertical_alignment_vector{ alignment_vectors[static_cast<std::size_t>(vertical_alignment)] };
		value = { 
			math::Floor(horizontal_size_vector.DotProduct(horizontal_alignment_vector)), 
			math::Floor(vertical_size_vector.DotProduct(vertical_alignment_vector)) 
		};
	}
	V2_int value;
};

} // namespace animation

} // namespace ptgn