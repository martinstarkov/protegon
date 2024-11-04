#pragma once

#include "math/vector2.h"

namespace ptgn {

struct Raycast {
	float t{ 1.0f }; // How far along the ray the impact occurred.
	V2_float normal; // Normal of the impact (normalised).

	[[nodiscard]] bool Occurred() const {
		PTGN_ASSERT(t >= 0.0f);
		return t >= 0.0f && t < 1.0f;
	}
};

} // namespace ptgn