#pragma once

#include <cstdint> // std::uint32_t
#include <cmath>   // std::nextafter
#include <random>  // std::minstd_rand, std::mt19937, std::uniform_int_distribution, ...
#include <limits>  // std::numeric_limits

#include "type_traits.h"

namespace ptgn {

/*
* Define RNG object by giving it a type to generate from
* and a range or seed for the distribution. 
* Upper and lower bounds of RNG range are both inclusive: [min, max]
* Use operator() on the RNG object to obtain new random numbers.
* @tparam T Type of number to generate
* @tparam E Type of rng engine to use (std::minstd_rand [default], std::mt19937, etc)
*/
template <typename T, typename E = std::minstd_rand,
	type_traits::arithmetic<T> = true>
class RNG {
public:
	// Default range seedless distribution.
    // Range: [0, 1] (inclusive).
	RNG() = default;

	// Default range seeded distribution.
    // Range: [0, 1] (inclusive).
	RNG(std::uint32_t seed) : generator_{ seed } {}

	// Custom range seedless distribution.
    // Range: [min, max] (inclusive).
	RNG(T min, T max) {
		if constexpr (std::is_floating_point_v<T>)
			// ensures inclusive range.
			distribution_ = { min, std::nextafter(max, std::numeric_limits<T>::epsilon()) };
		else
			distribution_ = { min, max };
	}

	// Custom range seeded distribution.
    // Range: [min, max] (inclusive).
	RNG(std::uint32_t seed, T min, T max) : generator_{ seed } {
		if constexpr (std::is_floating_point_v<T>)
			// ensures inclusive range.
			distribution_ = { min, std::nextafter(max, std::numeric_limits<T>::epsilon()) };
		else
			distribution_ = { min, max };
	}

	// Generate a new random number in the specified range.
	T operator()() {
		return distribution_(generator_);
	}

	// Change seed of random number generator.
	void SetSeed(std::uint32_t new_seed) {
		generator_.seed(new_seed);
	}
private:
	// Template which picks correct uniform distribution
	// generator based on the provided type.
	template <typename V>
	using uniform_distribution = 
		typename std::conditional<
		std::is_floating_point_v<V>, 
		std::uniform_real_distribution<V>,
		typename std::conditional<
		std::is_integral_v<V>, 
		std::uniform_int_distribution<V>, 
		void>::type>::type;

	// Internal random number generator.
	E generator_{ std::random_device{}() };

	// Defined internal distribution.
    // Range: [0, 1] (inclusive).
	uniform_distribution<T> distribution_{ 0, 1 };
};

} // namespace ptgn