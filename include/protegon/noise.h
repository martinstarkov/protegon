#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <utility>
#include <vector>

#include "protegon/math.h"
#include "protegon/rng.h"
#include "protegon/vector2.h"
#include "utility/debug.h"

namespace ptgn {

class ValueNoise {
public:
	ValueNoise() = default;

	ValueNoise(std::size_t size, std::uint32_t seed = 0) :
		float_rng{ seed },
		permutation_rng{ seed, 0, size - 1 },
		noise(size),
		permutations(size * 2) {
		std::generate(noise.begin(), noise.end(), [&]() { return float_rng(); });

		std::iota(permutations.begin(), permutations.end(), 0);

		for (std::size_t k{ 0 }; k < noise.size(); ++k) {
			std::size_t idx{ permutation_rng() };
			PTGN_ASSERT(idx < permutations.size(), idx);
			std::swap(permutations[k], permutations[idx]);
			permutations[k + noise.size()] = permutations[k];
		}
	}

	[[nodiscard]] float Evaluate(const V2_float& pos) const {
		auto xi = static_cast<int>(FastFloor(pos.x));
		auto yi = static_cast<int>(FastFloor(pos.y));

		float tx = pos.x - static_cast<float>(xi);
		float ty = pos.y - static_cast<float>(yi);

		int mask{ static_cast<int>(noise.size()) - 1 };
		int rx0 = xi & mask;
		int rx1 = (rx0 + 1) & mask;
		int ry0 = yi & mask;
		int ry1 = (ry0 + 1) & mask;

		// random values at the corners of the cell using permutation table
		const float& c00 = noise[permutations[permutations[rx0] + ry0]];
		const float& c10 = noise[permutations[permutations[rx1] + ry0]];
		const float& c01 = noise[permutations[permutations[rx0] + ry1]];
		const float& c11 = noise[permutations[permutations[rx1] + ry1]];

		// remapping of tx and ty using the Smoothstep function
		float sx = Smoothstep(tx);
		float sy = Smoothstep(ty);

		// linearly interpolate values along the x axis
		float nx0 = Lerp(c00, c10, sx);
		float nx1 = Lerp(c01, c11, sx);

		// linearly interpolate the nx0/nx1 along they y axis
		return Lerp(nx0, nx1, sy);
	}

private:
	RNG<float> float_rng;
	RNG<std::size_t> permutation_rng;

	std::vector<float> noise;
	std::vector<std::size_t> permutations; // size: noise.size() * 2
};

struct NoiseProperties {
	// Number of layers of noise added on top of each other.
	// Lower value means less higher frequency noise layers.
	std::size_t octaves{ 5 };
	// Sampling rate of the first layer of noise as a % of the provided noise array.
	// Lower value means the initial noise layer has a higher noise frequency.
	float frequency{ 0.03f };
	// Amount by which the sampling rate of each successive layer of noise is multiplied.
	// Lower value means the noise frequency of each noise layer increases slower.
	float bias{ 2.4f };
	// Amount by which the amplitude of each successive layer of noise is multiplied.
	// Lower value means less high frequency noise.
	float persistence{ 0.7f };

	[[nodiscard]] friend bool operator==(const NoiseProperties& a, const NoiseProperties& b) {
		return a.octaves == b.octaves && NearlyEqual(a.frequency, b.frequency) &&
			   NearlyEqual(a.bias, b.bias) && NearlyEqual(a.persistence, b.persistence);
	}

	[[nodiscard]] friend bool operator!=(const NoiseProperties& a, const NoiseProperties& b) {
		return !(a == b);
	}
};

class FractalNoise {
public:
	[[nodiscard]] static std::vector<float> Generate(
		const ValueNoise& noise, const V2_float& pos, const V2_int& size,
		const NoiseProperties& properties
	) {
		int length{ size.x * size.y };

		PTGN_ASSERT(length > 0);
		PTGN_ASSERT(properties.octaves > 0);
		PTGN_ASSERT(properties.frequency > 0);
		PTGN_ASSERT(properties.bias > 0);
		PTGN_ASSERT(properties.persistence > 0);

		float max_noise{ 0.0f };
		float amplitude{ 1.0f };

		std::vector<float> noise_map(length);

		for (std::size_t octave{ 0 }; octave < properties.octaves; ++octave) {
			max_noise += amplitude;
			amplitude *= properties.persistence;
		}

		for (std::size_t j{ 0 }; j < size.y; ++j) {
			for (std::size_t i{ 0 }; i < size.x; ++i) {
				V2_float noise_pos =
					(pos + V2_float{ static_cast<float>(i), static_cast<float>(j) }) *
					properties.frequency;
				amplitude = 1.0f;
				auto& local_noise{ noise_map[j * size.x + i] };

				for (std::size_t octave{ 0 }; octave < properties.octaves; ++octave) {
					local_noise += noise.Evaluate(noise_pos) * amplitude;
					noise_pos	*= properties.bias;
					amplitude	*= properties.persistence;
				}
			}
		}

		PTGN_ASSERT(max_noise >= 0);

		for (std::size_t i{ 0 }; i < length; ++i) {
			noise_map[i] /= max_noise;
		}

		return noise_map;
	}
};

} // namespace ptgn