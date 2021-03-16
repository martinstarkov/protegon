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
		r.resize(size_, 0);
		permutation_table_.resize(size_ * 2, 0);
		// create an array of random values and initialize permutation table
		for (unsigned k = 0; k < size_; ++k) {
			r[k] = flt_rng();
			permutation_table_[k] = k;
		}
		for (unsigned k = 0; k < size_; ++k) {
			unsigned i = uint_rng() & size_mask_;
			std::swap(permutation_table_[k], permutation_table_[i]);
			permutation_table_[k + size_] = permutation_table_[k];
		}
	}
	float Evaluate(const V2_float& p) {
		auto i = Floor(p);

		auto rx0 = i.x & size_mask_;
		auto rx1 = (rx0 + 1) & size_mask_;
		auto ry0 = i.y & size_mask_;
		auto ry1 = (ry0 + 1) & size_mask_;

		// random values at the corners of the cell using permutation table
		const auto& c00 = r[permutation_table_[permutation_table_[rx0] + ry0]];
		const auto& c10 = r[permutation_table_[permutation_table_[rx1] + ry0]];
		const auto& c01 = r[permutation_table_[permutation_table_[rx0] + ry1]];
		const auto& c11 = r[permutation_table_[permutation_table_[rx1] + ry1]];

		// remapping of coordinate fraction using the Smoothstep function 
		auto s = SmoothStep(p - i);

		// linearly interpolate values along the x axis
		auto nx0 = math::Lerp(c00, c10, s.x);
		auto nx1 = math::Lerp(c01, c11, s.x);

		// linearly interpolate the nx0/nx1 along they y axis
		return math::Lerp(nx0, nx1, s.y);
	}
	std::vector<float> GenerateNoiseMap(const V2_double& position, const V2_int& size, std::uint32_t octave, float bias) {

		float amplitude = 1;
		float maxPossibleNoiseVal = 0;
		float amplitudeMult = 0.5f;//0.35;
		unsigned numLayers = octave;//5;

		for (unsigned l = 0; l < numLayers; ++l) {
			maxPossibleNoiseVal += amplitude;
			amplitude *= amplitudeMult;
		}

		std::vector<float> noiseMap;
		noiseMap.resize(size.x * size.y, 0.0f);

		// FRACTAL NOISE
		float frequency = 0.05f;//0.02f;
		float frequencyMult = bias;//1.8;
		float maxNoiseVal = 0;
		for (int j = 0; j < size.y; ++j) {
			for (int i = 0; i < size.x; ++i) {
				V2_float pNoise{ position.x * size.x + i, position.y * size.y + j };
				pNoise *= frequency;
				amplitude = 1;
				for (unsigned l = 0; l < numLayers; ++l) {
					//LOG("pNoise: " << pNoise.x << "," << pNoise.y);
					auto fractal_noise = Evaluate(pNoise);
					auto value = fractal_noise * amplitude;
					if (std::isnan(value) && l > 10) value = 0;
					bool assertion = value >= 0;
					if (!assertion) {
						LOG("fractal_noise: " << value << ", pNoise: (" << pNoise.x << "," << pNoise.y << "), octave: " << l << ", amplitude: " << amplitude);
					}
					assert(assertion && "fractal_noise must be above or equal to 0");
					noiseMap[j * size.x + i] += value;
					pNoise *= frequencyMult;
					amplitude *= amplitudeMult;
				}
				if (noiseMap[j * size.y + i] > maxNoiseVal) maxNoiseVal = noiseMap[j * size.x + i];
			}
		}
		//for (unsigned i = 0; i < imageWidth * imageHeight; ++i) noiseMap[i] /= maxNoiseVal;
		for (int i = 0; i < size.x * size.y; ++i) {
			assert(noiseMap[i] >= 0 && "Noise must be above or equal to 0");
			noiseMap[i] = noiseMap[i] / maxPossibleNoiseVal;
			assert(noiseMap[i] >= 0 && "Noise divided by something which made it negative");
		}
		return noiseMap;
	}
	unsigned int size_;// = 256 * 2;
	unsigned int size_mask_;// = size_ - 1;
	std::vector<float> r; // size: size_
	std::vector<unsigned int> permutation_table_; // size_ * 2
};

} // namespace engine