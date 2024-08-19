#pragma once

#include <cmath>

#include "protegon/math.h"
#include "utility/debug.h"

using namespace ptgn;

bool TestMath() {
	PTGN_INFO("Starting math tests...");

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

	PTGN_ASSERT(aa1 == 0.0);
	PTGN_ASSERT(aa2 == 1.0);
	PTGN_ASSERT(aa3 == -1.0);
	PTGN_ASSERT(aa4 == -1.0);
	PTGN_ASSERT(aa5 == 1.0);

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

	PTGN_ASSERT(bb1 == 0);
	PTGN_ASSERT(bb2 == 1);
	PTGN_ASSERT(bb3 == -1);
	PTGN_ASSERT(bb4 == -1);
	PTGN_ASSERT(bb5 == 1);

	double c1  = 0.1;
	double c2  = 1.2;
	double c3  = -1.3;
	double c4  = -66.4;
	double c5  = 64.1;
	double c6  = 0.6;
	double c7  = 1.7;
	double c8  = -1.8;
	double c9  = -66.9;
	double c10 = 64.6;

	// FastFloor / FastCeil / FastAbs tests.
	// TODO: Add test cases for Inf/-Inf and Nan?

	PTGN_ASSERT(FastFloor(c1) == std::floor(c1));
	PTGN_ASSERT(FastFloor(c2) == std::floor(c2));
	PTGN_ASSERT(FastFloor(c3) == std::floor(c3));
	PTGN_ASSERT(FastFloor(c4) == std::floor(c4));
	PTGN_ASSERT(FastFloor(c5) == std::floor(c5));
	PTGN_ASSERT(FastFloor(c6) == std::floor(c6));
	PTGN_ASSERT(FastFloor(c7) == std::floor(c7));
	PTGN_ASSERT(FastFloor(c8) == std::floor(c8));
	PTGN_ASSERT(FastFloor(c9) == std::floor(c9));
	PTGN_ASSERT(FastFloor(c10) == std::floor(c10));

	PTGN_ASSERT(FastCeil(c1) == std::ceil(c1));
	PTGN_ASSERT(FastCeil(c2) == std::ceil(c2));
	PTGN_ASSERT(FastCeil(c3) == std::ceil(c3));
	PTGN_ASSERT(FastCeil(c4) == std::ceil(c4));
	PTGN_ASSERT(FastCeil(c5) == std::ceil(c5));
	PTGN_ASSERT(FastCeil(c6) == std::ceil(c6));
	PTGN_ASSERT(FastCeil(c7) == std::ceil(c7));
	PTGN_ASSERT(FastCeil(c8) == std::ceil(c8));
	PTGN_ASSERT(FastCeil(c9) == std::ceil(c9));
	PTGN_ASSERT(FastCeil(c10) == std::ceil(c10));

	PTGN_ASSERT(FastAbs(c1) == std::abs(c1));
	PTGN_ASSERT(FastAbs(c2) == std::abs(c2));
	PTGN_ASSERT(FastAbs(c3) == std::abs(c3));
	PTGN_ASSERT(FastAbs(c4) == std::abs(c4));
	PTGN_ASSERT(FastAbs(c5) == std::abs(c5));
	PTGN_ASSERT(FastAbs(c6) == std::abs(c6));
	PTGN_ASSERT(FastAbs(c7) == std::abs(c7));
	PTGN_ASSERT(FastAbs(c8) == std::abs(c8));
	PTGN_ASSERT(FastAbs(c9) == std::abs(c9));
	PTGN_ASSERT(FastAbs(c10) == std::abs(c10));

	// NearlyEqual tests.

	float test1{ 0.003f };
	float test2{ 0.007f };
	float test3{ test1 + test2 };
	float test4{ test3 / 2.0f };
	float test5{ test4 / 3.0f };
	PTGN_ASSERT(NearlyEqual(test1 + test2, test3));
	PTGN_ASSERT(NearlyEqual((test1 + test2) / 2.0f, test4));
	PTGN_ASSERT(NearlyEqual(((test1 + test2) / 2.0f) / 3.0f, test5));

	PTGN_ASSERT(NearlyEqual(0, 0));
	PTGN_ASSERT(NearlyEqual(0.0, 0.0));
	PTGN_ASSERT(NearlyEqual(0.0f, 0.0f));
	PTGN_ASSERT(NearlyEqual(0.1f, 0.1f));
	PTGN_ASSERT(NearlyEqual(0.001f, 0.001f));
	PTGN_ASSERT(NearlyEqual(-0.0f, -0.0f));
	PTGN_ASSERT(NearlyEqual(-0.1f, -0.1f));
	PTGN_ASSERT(NearlyEqual(-0.001f, -0.001f));
	PTGN_ASSERT(NearlyEqual(1000.0f, 1000.0f));
	PTGN_ASSERT(NearlyEqual(1000.01f, 1000.01f));
	PTGN_ASSERT(NearlyEqual(1000.0001f, 1000.0001f));
	PTGN_ASSERT(NearlyEqual(-1000.0f, -1000.0f));
	PTGN_ASSERT(NearlyEqual(-1000.01f, -1000.01f));
	PTGN_ASSERT(NearlyEqual(-1000.0001f, -1000.0001f));

	PTGN_ASSERT(NearlyEqual(0.0, 0.0 + 0.0049));
	PTGN_ASSERT(NearlyEqual(0.0f, 0.0f + 0.0049f));
	PTGN_ASSERT(NearlyEqual(0.1f, 0.1f + 0.0049f));
	PTGN_ASSERT(NearlyEqual(0.001f, 0.001f + 0.0049f));
	PTGN_ASSERT(NearlyEqual(-0.0f, -0.0f - 0.0049f));
	PTGN_ASSERT(NearlyEqual(-0.1f, -0.1f - 0.0049f));
	PTGN_ASSERT(NearlyEqual(-0.001f, -0.001f - 0.0049f));
	PTGN_ASSERT(NearlyEqual(-0.0f, -0.0f + 0.0049f));
	PTGN_ASSERT(NearlyEqual(-0.1f, -0.1f + 0.0049f));
	PTGN_ASSERT(NearlyEqual(-0.001f, -0.001f + 0.0049f));
	PTGN_ASSERT(NearlyEqual(1000.0f, 1000.0f + 0.0049f));
	PTGN_ASSERT(NearlyEqual(1000.01f, 1000.01f + 0.0049f));
	PTGN_ASSERT(NearlyEqual(1000.0001f, 1000.0001f + 0.0049f));
	PTGN_ASSERT(NearlyEqual(-1000.0f, -1000.0f - 0.0049f));
	PTGN_ASSERT(NearlyEqual(-1000.01f, -1000.01f - 0.0049f));
	PTGN_ASSERT(NearlyEqual(-1000.0001f, -1000.0001f - 0.0049f));
	PTGN_ASSERT(NearlyEqual(-1000.0f, -1000.0f + 0.0049f));
	PTGN_ASSERT(NearlyEqual(-1000.01f, -1000.01f + 0.0049f));
	PTGN_ASSERT(NearlyEqual(-1000.0001f, -1000.0001f + 0.0049f));

	PTGN_ASSERT(!NearlyEqual(0, 1));
	PTGN_ASSERT(!NearlyEqual(0.0, 0.0 + 0.0065));
	PTGN_ASSERT(!NearlyEqual(0.0f, 0.0f + 0.0065f));

	// TODO: Add tests for:
	// ToRad()
	// ToDeg()
	// ClampAngle360()
	// ClampAngle2Pi()
	// QuadraticFormula()

	PTGN_INFO("All math tests passed!");
	return true;
}