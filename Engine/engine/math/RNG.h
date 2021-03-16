#pragma once

#include <cstdint>
#include <random>

#include "utils/TypeTraits.h"

namespace engine {

// TODO: Write tests for E randon number engines.

/*
* @tparam T Type of number to generate
* @tparam E Type of engine to use (std::minstd_rand [default], std::mt19937, etc)
*/
template <typename T, typename E = std::minstd_rand,
	type_traits::is_number<T> = true>
class RNG {
public:
	RNG() = default;
	// Seed only constructor.
	RNG(std::uint32_t seed) : gen{ seed } {}
	// Range only constructor.
	RNG(T min, T max) : dist{ min, max } {}
	// Seed and range constructor.
	RNG(std::uint32_t seed, T min, T max) : gen{ seed }, dist{ min, max } {}
	
	T operator()() {
		return dist(gen);
	}

	void SetSeed(std::uint32_t new_seed) {
		gen.seed(new_seed);
		//seed32 = new_seed;
	}
private:
	template <typename T>
	using uniform_distribution =
		typename std::conditional<
		std::is_floating_point<T>::value,
		std::uniform_real_distribution<T>,
		typename std::conditional<
		std::is_integral<T>::value,
		std::uniform_int_distribution<T>,
		void
		>::type
		>::type;
	E gen{ std::random_device{}() };
	uniform_distribution<T> dist;

	/*
	// Lehmer32 generator found in OLC's procedural universe generator:
	// https://github.com/OneLoneCoder/olcPixelGameEngine/blob/master/Videos/OneLoneCoder_PGE_ProcGen_Universe.cpp
	template <typename T,
		type_traits::is_floating_point<T> = true,
		type_traits::convertible<T, double> = true
	>
	T Random(T min = 0.0, T max = 1.0) {
		assert(max > min && "Range must contain at least one double inside it");
		//std::uniform_real_distribution<double> dist(min, max);
		//return dist(gen);
		return ((T)Lehmer32() / (T)(std::numeric_limits<std::uint32_t>::max())) * (max - min) + min;
	}
	template <typename T, 
		type_traits::is_integral<T> = true,
		type_traits::convertible<T, int> = true
	>
	T Random(T min = 0, T max = 1) {
		assert(max > min && "Range must have at least one integer inside it");
		//std::uniform_int_distribution<int> dist(min, max);
		//return dist(gen);
		return (Lehmer32() % (max - min)) + min;
	}
	std::uint32_t seed32{ 0 };
	std::uint32_t Lehmer32() {
		seed32 += 0xe120fc15;
		std::uint64_t tmp;
		tmp = (std::uint64_t)seed32 * 0x4a39b70d;
		std::uint32_t m1 = (std::uint32_t)((tmp >> 32) ^ tmp);
		tmp = (std::uint64_t)m1 * 0x12fad5c9;
		std::uint32_t m2 = (std::uint32_t)((tmp >> 32) ^ tmp);
		return m2;
	}
	std::uint64_t seed64 = 0;
	// Lehmer64 generator found in Daniel Lemire's blog post:
	// https://lemire.me/blog/2019/03/19/the-fastest-conventional-random-number-generator-that-can-pass-big-crush/
	std::uint64_t Lehmer64() {
		seed64 *= 0xda942042e4dd58b5;
		return seed64 >> 64;
	}
	*/
};

} // namespace engine