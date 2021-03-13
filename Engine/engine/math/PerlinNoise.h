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
	ValueNoise(unsigned size, unsigned seed = 2021) : kMaxTableSize{ size }, kMaxTableSizeMask{ kMaxTableSize - 1 } {
		std::mt19937 gen(seed);
		std::uniform_real_distribution<float> distrFloat;
		auto randFloat = std::bind(distrFloat, gen);
		r.resize(kMaxTableSize, 0);
		permutationTable.resize(kMaxTableSize * 2, 0);
		// create an array of random values and initialize permutation table
		for (unsigned k = 0; k < kMaxTableSize; ++k) {
			r[k] = randFloat();
			permutationTable[k] = k;
		}
		// shuffle values of the permutation table
		std::uniform_int_distribution<unsigned> distrUInt;
		auto randUInt = std::bind(distrUInt, gen);
		for (unsigned k = 0; k < kMaxTableSize; ++k) {
			unsigned i = randUInt() & kMaxTableSizeMask;
			std::swap(permutationTable[k], permutationTable[i]);
			permutationTable[k + kMaxTableSize] = permutationTable[k];
		}
	}
	float Evaluate(const V2_float& p) {
		int xi = math::Floor(p.x);
		int yi = math::Floor(p.y);

		float tx = p.x - xi;
		float ty = p.y - yi;

		int rx0 = xi & kMaxTableSizeMask;
		int rx1 = (rx0 + 1) & kMaxTableSizeMask;
		int ry0 = yi & kMaxTableSizeMask;
		int ry1 = (ry0 + 1) & kMaxTableSizeMask;

		// random values at the corners of the cell using permutation table
		const float& c00 = r[permutationTable[permutationTable[rx0] + ry0]];
		const float& c10 = r[permutationTable[permutationTable[rx1] + ry0]];
		const float& c01 = r[permutationTable[permutationTable[rx0] + ry1]];
		const float& c11 = r[permutationTable[permutationTable[rx1] + ry1]];

		// remapping of tx and ty using the Smoothstep function 
		float sx = math::SmoothStep(tx);
		float sy = math::SmoothStep(ty);

		// linearly interpolate values along the x axis
		float nx0 = math::Lerp(c00, c10, sx);
		float nx1 = math::Lerp(c01, c11, sx);

		// linearly interpolate the nx0/nx1 along they y axis
		return math::Lerp(nx0, nx1, sy);
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
	unsigned int kMaxTableSize;// = 256 * 2;
	unsigned int kMaxTableSizeMask;// = kMaxTableSize - 1;
	std::vector<float> r; // size: kMaxTableSize
	std::vector<unsigned int> permutationTable; // kMaxTableSize * 2
};

} // namespace engine