#pragma once

#include <ostream>

#include "core/math/vector2.h"

namespace ptgn {

struct Viewport {
	/// @brief Top left position.
	V2_int position;
	V2_int size;

	V2_float GetCenter() const {
		return position + size * 0.5f;
	}

	bool operator==(const Viewport&) const = default;

	friend std::ostream& operator<<(std::ostream& o, const Viewport& viewport) {
		o << "[pos=";
		o << viewport.position;
		o << ",size=";
		o << viewport.size;
		o << "]";
		return o;
	}
};

} // namespace ptgn