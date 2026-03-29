#pragma once

#include <ostream>

#include "core/math/vector2.h"

namespace ptgn {

struct Viewport {
	/// @brief Position of the top left of the viewport relative to the display render target.
	V2_int position;
	/// @brief Size of the viewport in pixels.
	V2_int size;

	V2_float GetCenter() const {
		return position + size * 0.5f;
	}

	bool operator==(const Viewport&) const = default;

	friend std::ostream& operator<<(std::ostream& os, const Viewport& viewport) {
		os << "{ pos: " << viewport.position;
		os << ", size: " << viewport.size << "}";
		return os;
	}
};

} // namespace ptgn