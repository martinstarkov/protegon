#pragma once

#include "math/vector2.h"

namespace ptgn {

struct Intersection {
	float depth{ 0.0f };
	V2_float normal;

	[[nodiscard]] bool Occurred() const;
};

} // namespace ptgn