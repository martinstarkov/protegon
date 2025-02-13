#pragma once

#include "math/vector2.h"

namespace ptgn {

struct Transform {
	Transform() = default;

	Transform(
		const V2_float& position, float rotation = 0.0f, const V2_float& scale = { 1.0f, 1.0f }
	) :
		position{ position }, rotation{ rotation }, scale{ scale } {}

	V2_float position;
	float rotation{ 0.0f };
	V2_float scale{ 1.0f, 1.0f };
};

} // namespace ptgn