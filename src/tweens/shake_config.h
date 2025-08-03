#pragma once

#include "math/math.h"
#include "math/vector2.h"
#include "serialization/serializable.h"

namespace ptgn {

struct ShakeConfig {
	// Maximum translation distance during shaking.
	V2_float maximum_translation{ 30.0f, 30.0f };

	// Maximum rotation (in radians) during shaking.
	float maximum_rotation{ DegToRad(30.0f) };

	// Frequency of the Perlin noise function. Higher values will result in faster shaking.
	float frequency{ 10.0f };

	// Trauma is taken to this power before shaking is applied. Higher values will result in a
	// smoother falloff as trauma reduces.
	float trauma_exponent{ 2.0f };

	// Amount of trauma per second that is recovered.
	float recovery_speed{ 0.5f };

	friend bool operator==(const ShakeConfig& a, const ShakeConfig& b) {
		return a.maximum_translation == b.maximum_translation &&
			   NearlyEqual(a.maximum_rotation, b.maximum_rotation) &&
			   NearlyEqual(a.frequency, b.frequency) &&
			   NearlyEqual(a.trauma_exponent, b.trauma_exponent) &&
			   NearlyEqual(a.recovery_speed, b.recovery_speed);
	}

	friend bool operator!=(const ShakeConfig& a, const ShakeConfig& b) {
		return !(a == b);
	}

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(
		ShakeConfig, maximum_translation, maximum_rotation, frequency, trauma_exponent,
		recovery_speed
	)
};

} // namespace ptgn