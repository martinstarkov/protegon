#include "core/math/rng.h"

#include <algorithm>
#include <cstdint>

#include "core/math/math_utils.h"

namespace ptgn {

bool FlipCoin() {
	static RNG<int> rng{ 0, 1 };
	return rng();
}

std::size_t RandomSize(std::size_t min, std::size_t max) {
	RNG<std::size_t> rng{ min, max };
	return rng();
}

int RandomInt(int min, int max) {
	RNG<int> rng{ min, max };
	return rng();
}

float RandomFloat(float min, float max) {
	RNG<float> rng{ min, max };
	return rng();
}

std::uint8_t Random0255() {
	static RNG<std::uint16_t> rng{ 0, 255 };
	return static_cast<std::uint8_t>(rng());
}

float Random11() {
	static RNG<float> rng{ -1.0f, 1.0f };
	return rng();
}

float Random01() {
	static RNG<float> rng{ 0.0f, 1.0f };
	return rng();
}

bool Chance(float probability) {
	probability = std::clamp(probability, 0.0f, 1.0f);
	if (probability <= 0.0f) {
		return false;
	}
	if (probability >= 1.0f) {
		return true;
	}
	return Random01() <= probability;
}

float RandomAngle() {
	static RNG<float> rng{ 0.0f, 360.0f };
	return rng();
}

float RandomAngleRadians() {
	static RNG<float> rng{ 0.0f, kTwoPi };
	return rng();
}

} // namespace ptgn