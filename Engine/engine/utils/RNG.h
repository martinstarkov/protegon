#pragma once

#include <cstdint>
#include <float.h>

namespace engine {

class RNG {
public:
	double RandomDouble(double min, double max) {
		return ((double)Lehmer32() / (double)(0x7FFFFFFF)) * (max - min) + min;
	}
	int RandomInt(int min, int max) {
		return (Lehmer32() % (max - min)) + min;
	}
	void SetSeed(std::uint32_t new_seed) {
		seed32 = new_seed;
	}
private:
	std::uint32_t seed32 = 0;
	// Lehmer32 generator found in OLC's procedural universe generator:
	// https://github.com/OneLoneCoder/olcPixelGameEngine/blob/master/Videos/OneLoneCoder_PGE_ProcGen_Universe.cpp
	std::uint32_t Lehmer32() {
		seed32 += 0xe120fc15;
		std::uint64_t tmp;
		tmp = static_cast<std::uint64_t>(seed32) * 0x4a39b70d;
		std::uint32_t m1 = static_cast<uint32_t>((tmp >> 32) ^ tmp);
		tmp = static_cast<std::uint64_t>(m1) * 0x12fad5c9;
		std::uint32_t m2 = static_cast<uint32_t>((tmp >> 32) ^ tmp);
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