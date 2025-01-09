#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

#include "core/game.h"
#include "math/math.h"
#include "math/noise.h"
#include "math/rng.h"
#include "math/vector2.h"

namespace ptgn {

// Based on: https://roystan.net/articles/camera-shake/

struct CameraShake {
	CameraShake() {
		using type = decltype(seed);
		RNG<type> rng_float{ std::numeric_limits<type>::min(), std::numeric_limits<type>::max() };
		seed = rng_float();
	}

	// Current offset from transform position.
	V2_float local_position;

	// Current offset from transform rotation (in radians).
	float local_rotation{ 0.0f };

	// Needs to be called once a frame to update the translation and rotation of the camera shake.
	void Update() {
		// Taking trauma to an exponent allows the ability to smoothen
		// out the transition from shaking to being static.
		float shake = std::pow(trauma, trauma_exponent);

		float dt   = game.dt();
		float time = game.time();

		local_position =
			V2_float{ maximum_translation.x *
						  (PerlinNoise::GetValue(time * frequency, 0.0f, seed + 0) * 2 - 1),
					  maximum_translation.y *
						  (PerlinNoise::GetValue(time * frequency, 0.0f, seed + 1) * 2 - 1) } *
			shake;

		local_rotation = maximum_rotation *
						 (PerlinNoise::GetValue(time * frequency, 0.0f, seed + 3) * 2 - 1) * shake;

		trauma = std::clamp(trauma - recovery_speed * dt, 0.0f, 1.0f);
	}

	// Resets camera shake back to 0.
	void Reset() {
		trauma = 0.0f;
	}

	// @param stress Value between 0 and 1 which determines how much current trauma changes.
	void Induce(float stress) {
		trauma = std::clamp(trauma + stress, 0.0f, 1.0f);
	}

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
private:
	// Value between 0 and 1 defining the current amount
	// of stress this transform is enduring.
	float trauma{ 0.0f };
	std::int32_t seed{ 0 };
};

} // namespace ptgn
