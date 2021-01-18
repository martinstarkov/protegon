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

template<typename T>
class Vec2 {
public:
	Vec2() : x(T(0)), y(T(0)) {}
	Vec2(T xx, T yy) : x(xx), y(yy) {}
	Vec2 operator * (const T& r) const { return Vec2(x * r, y * r); }
	Vec2& operator *= (const T& r) { x *= r, y *= r; return *this; }
	T x, y;
};

typedef Vec2<float> Vec2f;

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
	float eval(Vec2f& p) {
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

} // namespace engine