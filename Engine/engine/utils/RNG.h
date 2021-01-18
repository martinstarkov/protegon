#pragma once

#include <cstdint>
#include <float.h>
#include <random>
#include <cassert>

namespace engine {

class RNG {
public:
	double RandomDouble(double min, double max) {
		assert(max > min && "Range must contain at least one double inside it");
		//std::uniform_real_distribution<double> dist(min, max);
		//return dist(gen);
		return ((double)Lehmer32() / (double)(UINT32_MAX)) * (max - min) + min;
	}
	int RandomInt(int min, int max) {
		assert(max > min && "Range must have at least one integer inside it");
		/*std::uniform_int_distribution<int> dist(min, max);
		return dist(gen);*/
		return (Lehmer32() % (max - min)) + min;
	}
	void SetSeed(std::uint32_t new_seed) {
		gen.seed(new_seed);
		seed32 = new_seed;
	}
private:
	std::minstd_rand gen;

	// Lehmer32 generator found in OLC's procedural universe generator:
	// https://github.com/OneLoneCoder/olcPixelGameEngine/blob/master/Videos/OneLoneCoder_PGE_ProcGen_Universe.cpp
	std::uint32_t seed32 = 0;
	uint32_t Lehmer32() {
		seed32 += 0xe120fc15;
		uint64_t tmp;
		tmp = (uint64_t)seed32 * 0x4a39b70d;
		uint32_t m1 = (uint32_t)((tmp >> 32) ^ tmp);
		tmp = (uint64_t)m1 * 0x12fad5c9;
		uint32_t m2 = (uint32_t)((tmp >> 32) ^ tmp);
		return m2;
	}
	/*
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