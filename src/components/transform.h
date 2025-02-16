#pragma once

#include "math/vector2.h"
#include "serialization/fwd.h"

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

void to_json(json& j, const Transform& t);

void from_json(const json& j, Transform& t);

} // namespace ptgn