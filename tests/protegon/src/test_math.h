#pragma once

#include "protegon/math.h"

#include <cassert>  // assert
#include <cmath>    // std::floor
#include <iostream> // std::cout

using namespace ptgn;

bool TestMath() {
	std::cout << "Starting math tests..." << std::endl;

	// Sign tests.

	double a1 = 0.0;
	double a2 = 1.0;
	double a3 = -1.0;
	double a4 = -66.0;
	double a5 = 64.0;

	double aa1 = Sign(a1);
	double aa2 = Sign(a2);
	double aa3 = Sign(a3);
	double aa4 = Sign(a4);
	double aa5 = Sign(a5);

	assert(aa1 == 0.0);
	assert(aa2 == 1.0);
	assert(aa3 == -1.0);
	assert(aa4 == -1.0);
	assert(aa5 == 1.0);

	int b1 = 0;
	int b2 = 1;
	int b3 = -1;
	int b4 = -66;
	int b5 = 64;

	int bb1 = Sign(b1);
	int bb2 = Sign(b2);
	int bb3 = Sign(b3);
	int bb4 = Sign(b4);
	int bb5 = Sign(b5);

	assert(bb1 == 0);
	assert(bb2 == 1);
	assert(bb3 == -1);
	assert(bb4 == -1);
	assert(bb5 == 1);

	double c1 = 0.1;
	double c2 = 1.2;
	double c3 = -1.3;
	double c4 = -66.4;
	double c5 = 64.1;
	double c6 = 0.6;
	double c7 = 1.7;
	double c8 = -1.8;
	double c9 = -66.9;
	double c10 = 64.6;

	// FastFloor / FastCeil / FastAbs tests.

	assert(FastFloor(c1) == std::floor(c1));
	assert(FastFloor(c2) == std::floor(c2));
	assert(FastFloor(c3) == std::floor(c3));
	assert(FastFloor(c4) == std::floor(c4));
	assert(FastFloor(c5) == std::floor(c5));
	assert(FastFloor(c6) == std::floor(c6));
	assert(FastFloor(c7) == std::floor(c7));
	assert(FastFloor(c8) == std::floor(c8));
	assert(FastFloor(c9) == std::floor(c9));
	assert(FastFloor(c10) == std::floor(c10));

	assert(FastCeil(c1) == std::ceil(c1));
	assert(FastCeil(c2) == std::ceil(c2));
	assert(FastCeil(c3) == std::ceil(c3));
	assert(FastCeil(c4) == std::ceil(c4));
	assert(FastCeil(c5) == std::ceil(c5));
	assert(FastCeil(c6) == std::ceil(c6));
	assert(FastCeil(c7) == std::ceil(c7));
	assert(FastCeil(c8) == std::ceil(c8));
	assert(FastCeil(c9) == std::ceil(c9));
	assert(FastCeil(c10) == std::ceil(c10));

	assert(FastAbs(c1) == std::abs(c1));
	assert(FastAbs(c2) == std::abs(c2));
	assert(FastAbs(c3) == std::abs(c3));
	assert(FastAbs(c4) == std::abs(c4));
	assert(FastAbs(c5) == std::abs(c5));
	assert(FastAbs(c6) == std::abs(c6));
	assert(FastAbs(c7) == std::abs(c7));
	assert(FastAbs(c8) == std::abs(c8));
	assert(FastAbs(c9) == std::abs(c9));
	assert(FastAbs(c10) == std::abs(c10));

	// NearlyEqual tests.

	float test1{ 0.003f };
	float test2{ 0.007f };
	float test3{ test1 + test2 };
	float test4{ test3 / 2.0f };
	float test5{ test4 / 3.0f };
	assert(NearlyEqual(test1 + test2, test3));
	assert(NearlyEqual((test1 + test2) / 2.0f, test4));
	assert(NearlyEqual(((test1 + test2) / 2.0f) / 3.0f, test5));

	assert(NearlyEqual(0, 0));
	assert(NearlyEqual(0.0, 0.0));
	assert(NearlyEqual(0.0f, 0.0f));
	assert(NearlyEqual(0.1f, 0.1f));
	assert(NearlyEqual(0.001f, 0.001f));
	assert(NearlyEqual(-0.0f, -0.0f));
	assert(NearlyEqual(-0.1f, -0.1f));
	assert(NearlyEqual(-0.001f, -0.001f));
	assert(NearlyEqual(1000.0f, 1000.0f));
	assert(NearlyEqual(1000.01f, 1000.01f));
	assert(NearlyEqual(1000.0001f, 1000.0001f));
	assert(NearlyEqual(-1000.0f, -1000.0f));
	assert(NearlyEqual(-1000.01f, -1000.01f));
	assert(NearlyEqual(-1000.0001f, -1000.0001f));

	assert(NearlyEqual(0.0, 0.0 + 0.0049));
	assert(NearlyEqual(0.0f, 0.0f + 0.0049f));
	assert(NearlyEqual(0.1f, 0.1f + 0.0049f));
	assert(NearlyEqual(0.001f, 0.001f + 0.0049f));
	assert(NearlyEqual(-0.0f, -0.0f - 0.0049f));
	assert(NearlyEqual(-0.1f, -0.1f - 0.0049f));
	assert(NearlyEqual(-0.001f, -0.001f - 0.0049f));
	assert(NearlyEqual(-0.0f, -0.0f + 0.0049f));
	assert(NearlyEqual(-0.1f, -0.1f + 0.0049f));
	assert(NearlyEqual(-0.001f, -0.001f + 0.0049f));
	assert(NearlyEqual(1000.0f, 1000.0f + 0.0049f));
	assert(NearlyEqual(1000.01f, 1000.01f + 0.0049f));
	assert(NearlyEqual(1000.0001f, 1000.0001f + 0.0049f));
	assert(NearlyEqual(-1000.0f, -1000.0f - 0.0049f));
	assert(NearlyEqual(-1000.01f, -1000.01f - 0.0049f));
	assert(NearlyEqual(-1000.0001f, -1000.0001f - 0.0049f));
	assert(NearlyEqual(-1000.0f, -1000.0f + 0.0049f));
	assert(NearlyEqual(-1000.01f, -1000.01f + 0.0049f));
	assert(NearlyEqual(-1000.0001f, -1000.0001f + 0.0049f));

	assert(!NearlyEqual(0, 1));
	assert(!NearlyEqual(0.0, 0.0 + 0.0065));
	assert(!NearlyEqual(0.0f, 0.0f + 0.0065f));

	std::cout << "All math tests passed!" << std::endl;
	return true;
}