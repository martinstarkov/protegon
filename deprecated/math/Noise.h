#pragma once

#include <cstdint> // std::uint32_t, etc
#include <vector> // std::vector

#include "math/RNG.h"
#include "math/Math.h"
#include "math/Vector2.h"
#include "utils/TypeTraits.h"

namespace ptgn {

/*
* @tparam T Type of floating point noise values to generate. Float by default.
*/
template <typename T = float, 
	type_traits::is_floating_point_e<T> = true>
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
private:

	// Noise interpolation routine. Linear for now.
	T Interpolate(T a, T b, T amount) const {
		return math::Lerp(a, b, amount);
	}

	// Returns the smooth noise value at a given point.
	T Noise(const Vector2<T>& point) const {
		V2_int integer{ Floor(point) };

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

	V2_int size_;
	std::size_t length_;
	std::size_t length_mask_;
	std::vector<T> random_values_;
	std::vector<std::size_t> permutation_table_;
};

} // namespace ptgn