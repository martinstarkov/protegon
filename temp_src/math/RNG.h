#pragma once

#include <cstdint> // std::uint32_t, etc
#include <random> // std::minstd_rand, std::mt19937, std::uniform_int_distribution, etc

namespace ptgn {

namespace math {

/*
* Define RNG object by giving it a type to generate from
* and a range or seed for the distribution. 
* Upper and lower bounds of RNG range are respectively:
* [a, b) for real numbers, [a, b] for integers, [] = included, () = excluded.
* Use operator() on the RNG object to obtain new random numbers.
* @tparam T Type of number to generate
* @tparam E Type of rng engine to use (std::minstd_rand [default], std::mt19937, etc)
*/
template <typename T, typename E = std::minstd_rand,
	std::enable_if_t<std::is_arithmetic_v<T>, bool> = true>
class RNG {
public:
	// Default constructor makes distribution range 0 to 1.
	RNG() = default;
	// Seed only constructor.
	RNG(std::uint32_t seed) : generator{ seed } {}
	// Range only constructor.
	RNG(T min, T max) : distribution{ min, max } {}
	// Seed and range constructor.
	RNG(std::uint32_t seed, T min, T max) : gen{ seed }, distribution{ min, max } {}

	// Generate a new random number.
	T operator()() {
		return distribution(generator);
	}

	// Change seed of random number generator.
	void SetSeed(std::uint32_t new_seed) {
		generator.seed(new_seed);
	}
private:
	// Template which picks correct uniform distribution
	// generator based on the provided type.
	template <typename V>
	using uniform_distribution = 
		typename std::conditional<
		std::is_floating_point<V>::value, 
		std::uniform_real_distribution<V>,
		typename std::conditional<
		std::is_integral<V>::value, 
		std::uniform_int_distribution<V>, 
		void>::type>::type;
	// Internal random number generator.
	E generator{ std::random_device{}() };
	// Define internal distribution.
	// Range 0 to 1 by default.
	uniform_distribution<T> distribution{ 0, 1 };
};

} // namespace math

} // namespace ptgn