#pragma once

#include "RNG.h"

#include <cmath>
#include <random>
#include <algorithm>
#include <numeric>
#include <cstdio> 
#include <random> 
#include <functional> 
#include <iostream> 
#include <vector> 
#include <fstream> 

namespace engine {

class ValueNoise {
public:
	ValueNoise(unsigned size, unsigned seed = 2021) : 
		size_{ size }, 
		size_mask_{ size_ - 1 } {
		RNG<float> flt_rng{ seed };
		RNG<std::uint32_t> uint_rng{ seed };
		random_values.resize(size_, 0);
		permutation_table_.resize(size_ * 2, 0);
		// Create an array of random values and initialize permutation table.
		for (unsigned k = 0; k < size_; ++k) {
			random_values[k] = flt_rng();
			permutation_table_[k] = k;
		}
		// Shuffle permutation table.
		for (unsigned k = 0; k < size_; ++k) {
			unsigned i = uint_rng() & size_mask_;
			std::swap(permutation_table_[k], permutation_table_[i]);
			permutation_table_[k + size_] = permutation_table_[k];
		}
	}
	float Evaluate(const V2_float& p) {
		auto i{ Floor(p) };

		auto rx0{ i.x & size_mask_ };
		auto rx1{ (rx0 + 1) & size_mask_ };
		auto ry0{ i.y & size_mask_ };
		auto ry1{ (ry0 + 1) & size_mask_ };

		// Random values at the corners of the cells using the permutation table.
		auto c00{ random_values[permutation_table_[permutation_table_[rx0] + ry0]] };
		auto c10{ random_values[permutation_table_[permutation_table_[rx1] + ry0]] };
		auto c01{ random_values[permutation_table_[permutation_table_[rx0] + ry1]] };
		auto c11{ random_values[permutation_table_[permutation_table_[rx1] + ry1]] };

		// Remapping of coordinate fraction using the SmoothStep function.
		auto s{ SmoothStep(p - i) };

		// Linearly interpolate values along the x-axis.
		auto nx0{ math::Lerp(c00, c10, s.x) };
		auto nx1{ math::Lerp(c01, c11, s.x) };

		// Linearly interpolate result along the y-axis.
		return math::Lerp(nx0, nx1, s.y);
	}
	std::vector<float> GenerateNoiseMap(const V2_double& position, const V2_int& size, std::size_t octaves, float frequency_bias, float amplitude_bias = 0.5f) {

		auto frequency{ 0.05f };
		auto amplitude{ 1.0f };

		// Calculate maximum possible cumulative noise value.
		auto max_noise{ 0.0f };
		for (std::size_t octave{ 0 }; octave < octaves; ++octave) {
			max_noise += amplitude;
			amplitude *= amplitude_bias;
		}
		assert(max_noise >= 0 && "Noise cannot be standardized by negative values");

		std::vector<float> noise_map;
		std::size_t count{ static_cast<std::size_t>(size.x * size.y) };
		noise_map.resize(count, 0.0f);

		for (auto j{ 0 }; j < size.y; ++j) {
			for (auto i{ 0 }; i < size.x; ++i) {
				V2_float pos{ position.x + i, position.y + j };
				pos *= frequency;
				// Reset amplitude.
				amplitude = 1;
				for (std::size_t octave{ 0 }; octave < octaves; ++octave) {
					auto noise = Evaluate(pos) * amplitude;
					if (std::isnan(noise) && octave > 10) noise = 0;
					if (noise < 0) {
						LOG("noise: " << noise << ", position: " << pos << ", octave: " << octave << ", amplitude: " << amplitude);
						assert(!"Noise must be above or equal to 0");
					}
					noise_map[j * size.x + i] += noise;
					pos *= frequency_bias;
					amplitude *= amplitude_bias;
				}
			}
		}
		//for (unsigned i = 0; i < imageWidth * imageHeight; ++i) noiseMap[i] /= maxNoiseVal;
		// Standarise all noise values between 0 and 1.
		for (std::size_t i{ 0 }; i < count; ++i) {
			assert(noise_map[i] >= 0 && "Noise must be above or equal to 0");
			noise_map[i] /= max_noise;
		}
		return noise_map;
	}
	unsigned int size_;// = 256 * 2;
	unsigned int size_mask_;// = size_ - 1;
	std::vector<float> random_values; // size: size_
	std::vector<unsigned int> permutation_table_; // size_ * 2
};

} // namespace engine