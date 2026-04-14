#pragma once

#include "core/math/angle.h"
#include "core/math/math_utils.h"
#include "core/math/vector2.h"
#include "serialization/serialize.h"

namespace ptgn {

struct ShakeConfig {
	/// @brief Maximum translation distance during shaking.
	V2_float maximum_translation{ 30.0f, 30.0f };

	/// @brief Maximum rotation during shaking.
	Degrees maximum_rotation{ 30.0f };

	/// @brief Frequency of the Perlin noise function. Higher values will result in faster shaking.
	float frequency{ 10.0f };

	/// @brief Trauma is taken to this power before shaking is applied. Higher values will result in
	/// a smoother falloff as trauma reduces.
	float trauma_exponent{ 2.0f };

	/// @brief Amount of trauma per second that is recovered.
	float recovery_speed{ 0.5f };

	bool operator==(const ShakeConfig&) const = default;

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(
		ShakeConfig, maximum_translation, maximum_rotation, frequency, trauma_exponent,
		recovery_speed
	)
};

} // namespace ptgn