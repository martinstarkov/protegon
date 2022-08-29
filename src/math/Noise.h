#pragma once

#include <cstdint> // std::uint32_t, etc
#include <vector>  // std::vector
#include <cmath>   // std::lerp





#include <utility>   // 
#include <numeric>   // 
#include <random>   // 
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
#include <cstdint>
#include <float.h>
#include <random>
#include <cassert>




#include "math/RNG.h"
#include "math/Math.h"
#include "math/Vector2.h"
#include "utility/TypeTraits.h"

#include "interface/Draw.h"

namespace ptgn {

template<typename T = float>
inline T lerp(const T& lo, const T& hi, const T& t) {
	return lo * (1 - t) + hi * t;
}

inline float smoothstep(const float& t) {
	return t * t * (3 - 2 * t);
}

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
	float eval(V2_float& p) {
		int xi = (int)std::floor(p.x);
		int yi = (int)std::floor(p.y);

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
		float sx = smoothstep(tx);
		float sy = smoothstep(ty);

		// linearly interpolate values along the x axis
		float nx0 = lerp(c00, c10, sx);
		float nx1 = lerp(c01, c11, sx);

		// linearly interpolate the nx0/nx1 along they y axis
		return lerp(nx0, nx1, sy);
	}
	unsigned int kMaxTableSize;// = 256 * 2;
	unsigned int kMaxTableSizeMask;// = kMaxTableSize - 1;
	std::vector<float> r; // size: kMaxTableSize
	std::vector<unsigned int> permutationTable; // kMaxTableSize * 2
};

namespace math {

/*
* @tparam T Type of floating point noise values to generate. Float by default.
*/
template <typename T = float, 
	tt::floating_point<T> = true>
class ValueNoise {
public:
	/*
	* @pararm size Size of the noise map to generate.
	* @pararm seed Seed of internal random number generators.
	*/
	ValueNoise(const V2_int& size, std::uint32_t seed = 0) :
		size_{ size }, 
		length_{ static_cast<std::size_t>(size_.x) 
		* static_cast<std::size_t>(size_.y) },
		length_mask_{ length_ - 1 } {
		// Initialize random number generators.
		RNG<T> rng{ seed };
		RNG<std::size_t> size_rng{ seed };
		random_values_.resize(length_, 0);
		permutation_table_.resize(length_ * static_cast<std::size_t>(2), 0);
		// Create an array of random values and initialize permutation table.
		for (std::size_t i{ 0 }; i < length_; ++i) {
			random_values_[i] = rng();
			permutation_table_[i] = i;
		}
		// Shuffle permutation table.
		for (std::size_t k{ 0 }; k < length_; ++k) {
			auto i{ size_rng() & length_mask_ };
			std::swap(permutation_table_[k], permutation_table_[i]);
			permutation_table_[k + length_] = permutation_table_[k];
		}
	}

	~ValueNoise() = default;

	/* 
	* Generates a 2D noise map for points relative to a coordinate.
	* @param relative_position Point relative to which the map is generated.
	* @param octaves Number of layers of noise to sum.
	* @param frequency Initial frequency of noise layers.
	* @param lacunarity Increase of frequencies per layer (multiplier).
	* @param persistence Increase of amplitudes per layer (multiplier).
	* @return 2D noise map of values.
	*/
	std::vector<T> GenerateNoiseMap(const Vector2<T>& relative_position, std::size_t octaves, T frequency, T lacunarity, T persistence) const {
		
		// Initialize noise map.
		std::vector<T> noise_map;
		noise_map.resize(length_, 0);

		T max_noise{ 0 };
		T amplitude{ 1 };
		// Find maximum noise for given persistence.
		for (std::size_t octave{ 0 }; octave < octaves; ++octave) {
			max_noise += amplitude;
			amplitude *= persistence;
		}

		// Populate noise map.
		for (std::size_t y{ 0 }; y < size_.y; ++y) {
			auto index{ y * size_.x };
			for (std::size_t x{ 0 }; x < size_.x; ++x) {
				Vector2<T> coordinate{ x, y };
				// Find world coordinate of tile.
				coordinate += relative_position;
				coordinate *= frequency;
				// Reset amplitude.
				amplitude = 1;
				T total{ 0 };
				// Add layers of fractal noise together.
				for (std::size_t octave{ 0 }; octave < octaves; ++octave) {
					total += Noise(coordinate) * amplitude;
					// Adjust amplitude and frequency for next layer.
					coordinate *= lacunarity;
					amplitude *= persistence;
				}
				// Normalize noise.
				noise_map[index + x] = total / max_noise;
			}
		}

		return noise_map;
	}

	std::vector<float> Generate(V2_int grid_position) {
		int seed = 5;
		int octave = 5;
		double bias = 2.0;

		auto tiles = 2;

		float amplitude = 1;
		float maxPossibleNoiseVal = 0;
		float amplitudeMult = 0.5f;//0.35;
		unsigned numLayers = octave;//5;

		for (unsigned l = 0; l < numLayers; ++l) {
			maxPossibleNoiseVal += amplitude;
			amplitude *= amplitudeMult;
		}

		//LOG("maxPossibleNoiseVal: " << maxPossibleNoiseVal);

		ptgn::ValueNoise noise(256, seed);

		V2_int tile_size = { 16, 16 };
		V2_int tiles_per_chunk = { 16, 16 };
		V2_int chunk_size = tiles_per_chunk * tile_size;
		V2_int info_size = tiles_per_chunk;
		V2_int info_position = chunk_size * grid_position;

		unsigned imageWidth = (unsigned)info_size.x;
		unsigned imageHeight = (unsigned)info_size.y;

		V2_float overall = info_position / tile_size / info_size;

		std::vector<float> noiseMap(imageWidth * imageHeight, 0);

		// FRACTAL NOISE
		float frequency = 0.05f;//0.02f;
		float frequencyMult = (float)bias;//1.8;
		float maxNoiseVal = 0;
		for (unsigned j = 0; j < imageHeight; ++j) {
			for (unsigned i = 0; i < imageWidth; ++i) {
				V2_float pNoise = V2_float{ (float)overall.x * imageWidth + i, (float)overall.y * imageHeight + j } * frequency;
				amplitude = 1;
				for (unsigned l = 0; l < numLayers; ++l) {
					//LOG("pNoise: " << pNoise.x << "," << pNoise.y);
					auto fractal_noise = noise.eval(pNoise);
					auto value = fractal_noise * amplitude;
					if (std::isnan(value) && l > 10) value = 0;
					bool assertion = value >= 0;
					if (!assertion) {
						//LOG("fractal_noise: " << value << ", pNoise: (" << pNoise.x << "," << pNoise.y << "), octave: " << l << ", amplitude: " << amplitude);
					}
					assert(assertion && "fractal_noise must be above or equal to 0");
					noiseMap[j * imageWidth + i] += value;
					pNoise *= frequencyMult;
					amplitude *= amplitudeMult;
				}
				if (noiseMap[j * imageWidth + i] > maxNoiseVal) maxNoiseVal = noiseMap[j * imageWidth + i];
			}
		}
		//for (unsigned i = 0; i < imageWidth * imageHeight; ++i) noiseMap[i] /= maxNoiseVal;
		for (unsigned i = 0; i < imageWidth * imageHeight; ++i) {
			assert(noiseMap[i] >= 0 && "Noise must be above or equal to 0");
			noiseMap[i] = noiseMap[i] / maxPossibleNoiseVal;
			assert(noiseMap[i] >= 0 && "Noise divided by something which made it negative");
		}

		for (unsigned j = 0; j < imageHeight; ++j) {
			for (unsigned i = 0; i < imageWidth; ++i) {
				// generate a float in the range [0:1]
				auto raw_noise = noiseMap[j * imageWidth + i];
				auto noise = static_cast<unsigned char>(std::clamp(raw_noise * 255.0f, 0.0f, 255.0f));
				//auto noise = noiseMap[j * imageWidth + i];
				//engine::TextureManager::DrawPoint(, engine::Color(a, 0, 0, 255));
				auto tile = V2_int{ (int)i, (int)j };
				V2_double tile_position = tile * tile_size;
				V2_int absolute_tile_position = tile_position + info_position;
				Color color{ noise, 0, 0, 255 };
				AABB<int> aabb{ tile_position, tile_size };
				draw::SolidAABB(aabb, color);
			}
		}

		return noiseMap;
	}

private:

	// Noise interpolation routine. Linear for now.
	T Interpolate(T a, T b, T amount) const {
		return Lerp(a, b, amount);
	}

	// Returns the smooth noise value at a given point.
	T Noise(const Vector2<T>& point) const {
		V2_int integer{ FastFloor(point) };

		auto rx0{ integer.x & length_mask_ };
		auto rx1{ (rx0 + 1) & length_mask_ };
		auto ry0{ integer.y & length_mask_ };
		auto ry1{ (ry0 + 1) & length_mask_ };

		// Random values at the corners of the cells using the permutation table.
		auto c00{ random_values_[permutation_table_[permutation_table_[rx0] + ry0]] };
		auto c10{ random_values_[permutation_table_[permutation_table_[rx1] + ry0]] };
		auto c01{ random_values_[permutation_table_[permutation_table_[rx0] + ry1]] };
		auto c11{ random_values_[permutation_table_[permutation_table_[rx1] + ry1]] };

		// Remapping of coordinate fraction using the SmoothStep function.
		auto s{ SmoothStep(point - integer) };

		// Linearly interpolate values along the x-axis.
		auto nx0{ Interpolate(c00, c10, s.x) };
		auto nx1{ Interpolate(c01, c11, s.x) };

		// Linearly interpolate result along the y-axis.
		return Interpolate(nx0, nx1, s.y);
	}

	Vector2<T> SmoothStep(const Vector2<T>& v) const {
		return { ptgn::SmoothStep(v.x), ptgn::SmoothStep(v.y) };
	}

	V2_int size_;
	std::size_t length_;
	std::size_t length_mask_;
	std::vector<T> random_values_;
	std::vector<std::size_t> permutation_table_;
};

} // namespace math

} // namespace ptgn