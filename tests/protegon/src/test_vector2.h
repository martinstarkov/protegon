#pragma once

#include <limits>
#include <unordered_map>

#include "protegon/vector2.h"
#include "utility/debug.h"

using namespace ptgn;

bool TestVector2() {
	PTGN_INFO("Starting Vector2 tests...");

	// Implicit / explicit construction and copy tests.

	Vector2<int> t0{ 5, -2 };
	PTGN_ASSERT(t0.x == 5);
	PTGN_ASSERT(t0.y == -2);

	Vector2<int> t0b = -t0;
	PTGN_ASSERT(t0b.x == -5);
	PTGN_ASSERT(t0b.y == 2);
	PTGN_ASSERT(t0b == -t0);

	Vector2<int> t1{ 2, -1 };
	PTGN_ASSERT(t1.x == 2);
	PTGN_ASSERT(t1.y == -1);
	PTGN_ASSERT((t1 == Vector2<int>{ 2, -1 }));

	Vector2<double> t2{ 0, 1 };
	PTGN_ASSERT(t2.x == 0.0);
	PTGN_ASSERT(t2.y == 1.0);
	PTGN_ASSERT((t2 == Vector2<double>{ 0, 1 }));

	Vector2<double> t2b = -t2;
	PTGN_ASSERT(t2b.x == -0.0);
	PTGN_ASSERT(t2b.y == -1.0);
	PTGN_ASSERT(t2b == -t2);
	PTGN_ASSERT((t2b == Vector2<double>{ -0.0, -1.0 }));

	constexpr Vector2<double> t3{ -2.0, 3.0 };
	PTGN_ASSERT(t3.x == -2.0);
	PTGN_ASSERT(t3.y == 3.0);
	PTGN_ASSERT((t3 == Vector2<double>{ -2.0, 3.0 }));

	Vector2<float> t3a{ 0.0, 1.0 };
	Vector2<float> t3b = Vector2<float>{ 0.0, 1.0 };
	// Vector2<float> t3c = { 0.0, 1.0 }; // implicit narrowing
	Vector2<double> t3d{ 0.0f, 1.0f };
	Vector2<double> t3e = Vector2<double>{ 0.0f, 1.0f };
	Vector2<double> t3f = { 0.0f, 1.0f };

	Vector2<float> t4{ 0.0f, 1.0f };
	Vector2<double> t5{ 0.0, 1.0 };

	Vector2<float> t6a = t4;
	Vector2<float> t6b{ t4 };
	Vector2<double> t6c = t4;
	Vector2<double> t6e{ t4 };
	// Vector2<float>  t6f = t5; // implicit narrowing
	Vector2<float> t6g{ t5 };
	Vector2<double> t6h = t5;
	Vector2<double> t6i{ t5 };

	// Dot() tests.

	double t7a = t2.Dot(t1);
	double t7b = t2.Dot(t2);
	int t7c	   = t1.Dot(t0);
	// int t7d = t1.Dot(t2); // implicit narrowing
	PTGN_ASSERT(t7a - (-1) < std::numeric_limits<double>::epsilon());
	PTGN_ASSERT(t7b - (1) < std::numeric_limits<double>::epsilon());
	PTGN_ASSERT(t7c == 12);

	// Use in hashed container as keys test.

	std::unordered_map<Vector2<int>, int> map1;
	map1.emplace(t1, 3);
	PTGN_ASSERT(map1.find(t1)->second == 3);
	map1.emplace(t0, 2);
	PTGN_ASSERT(map1.find(t0)->second == 2);

	std::unordered_map<Vector2<double>, int> map2;
	map2.emplace(t2, 1);
	PTGN_ASSERT(map2.find(t2)->second == 1);
	map2.emplace(t2b, 5);
	PTGN_ASSERT(map2.find(t2b)->second == 5);
	PTGN_ASSERT(map2.find(t1) == map2.end());

	// += | -= | *= | /= operator tests.

	Vector2<double> s1{ 1.0, 2.0 };
	Vector2<double> s2{ 3.0, 4.0 };
	s1 *= s2;
	PTGN_ASSERT(s1.x == 1.0 * 3.0);
	PTGN_ASSERT(s1.y == 2.0 * 4.0);
	PTGN_ASSERT(s2.x == 3.0);
	PTGN_ASSERT(s2.y == 4.0);

	Vector2<int> s3{ 3, 4 };
	Vector2<double> s4{ 5.0, 6.0 };
	// s3 *= s4; // implicit narrowing
	s4 *= s3;
	PTGN_ASSERT(s4.x == 3 * 5.0);
	PTGN_ASSERT(s4.y == 4 * 6.0);
	PTGN_ASSERT(s3.x == 3);
	PTGN_ASSERT(s3.y == 4);

	Vector2<double> s5{ 1.0, 2.0 };
	Vector2<double> s6{ 3.0, 4.0 };
	s5 /= s6;
	PTGN_ASSERT(s5.x == 1.0 / 3.0);
	PTGN_ASSERT(s5.y == 2.0 / 4.0);
	PTGN_ASSERT(s6.x == 3.0);
	PTGN_ASSERT(s6.y == 4.0);

	Vector2<int> s7{ 3, 4 };
	Vector2<double> s8{ 5.0, 6.0 };
	// s7 /= s8; // implicit narrowing
	s8 /= s7;
	PTGN_ASSERT(s8.x == 5.0 / 3);
	PTGN_ASSERT(s8.y == 6.0 / 4);
	PTGN_ASSERT(s7.x == 3);
	PTGN_ASSERT(s7.y == 4);

	Vector2<double> s9{ 1.0, 2.0 };
	Vector2<double> s10{ 3.0, 4.0 };
	s9 -= s10;
	PTGN_ASSERT(s9.x == 1.0 - 3.0);
	PTGN_ASSERT(s9.y == 2.0 - 4.0);
	PTGN_ASSERT(s10.x == 3.0);
	PTGN_ASSERT(s10.y == 4.0);

	Vector2<int> s11{ 3, 4 };
	Vector2<double> s12{ 5.0, 6.0 };
	// s11 -= s12; // implicit narrowing
	s12 -= s11;
	PTGN_ASSERT(s12.x == 5.0 - 3);
	PTGN_ASSERT(s12.y == 6.0 - 4);
	PTGN_ASSERT(s11.x == 3);
	PTGN_ASSERT(s11.y == 4);

	Vector2<double> s13{ 1.0, 2.0 };
	Vector2<double> s14{ 3.0, 4.0 };
	s13 += s14;
	PTGN_ASSERT(s13.x == 1.0 + 3.0);
	PTGN_ASSERT(s13.y == 2.0 + 4.0);
	PTGN_ASSERT(s14.x == 3.0);
	PTGN_ASSERT(s14.y == 4.0);

	Vector2<int> s15{ 3, 4 };
	Vector2<double> s16{ 5.0, 6.0 };
	// s15 += s16; // implicit narrowing
	s16 += s15;
	PTGN_ASSERT(s16.x == 5.0 + 3);
	PTGN_ASSERT(s16.y == 6.0 + 4);
	PTGN_ASSERT(s15.x == 3);
	PTGN_ASSERT(s15.y == 4);

	Vector2<int> p1{ 3, 4 };
	Vector2<double> p2{ 3.0, 4.0 };
	int p3{ 5 };
	double p4{ 6.0 };
	p1 *= p3;
	// p1 *= p4; // // implicit narrowing
	PTGN_ASSERT(p1.x == 3 * 5);
	PTGN_ASSERT(p1.y == 4 * 5);
	PTGN_ASSERT(p3 == 5);
	p2 *= p3;
	PTGN_ASSERT(p2.x == 3.0 * 5);
	PTGN_ASSERT(p2.y == 4.0 * 5);
	PTGN_ASSERT(p3 == 5);
	p2 *= p4;
	PTGN_ASSERT(p2.x == 3.0 * 5 * 6.0);
	PTGN_ASSERT(p2.y == 4.0 * 5 * 6.0);
	PTGN_ASSERT(p4 == 6.0);

	Vector2<int> q1{ 3, 4 };
	Vector2<double> q2{ 3.0, 4.0 };
	int q3{ 5 };
	double q4{ 6.0 };
	q1 /= q3;
	// q1 /= q4; // // implicit narrowing
	PTGN_ASSERT(q1.x == 3 / 5);
	PTGN_ASSERT(q1.y == 4 / 5);
	PTGN_ASSERT(q3 == 5);
	q2 /= q3;
	PTGN_ASSERT(q2.x == 3.0 / 5);
	PTGN_ASSERT(q2.y == 4.0 / 5);
	PTGN_ASSERT(q3 == 5);
	q2 /= q4;
	PTGN_ASSERT(q2.x == 3.0 / 5 / 6.0);
	PTGN_ASSERT(q2.y == 4.0 / 5 / 6.0);
	PTGN_ASSERT(q4 == 6.0);

	// Rounded() tests.

	Vector2<double> r1r{ 1.3, 1.3 };
	Vector2<double> r2r{ 2.6, 2.6 };
	Vector2<double> r3r{ 3.5, 3.5 };
	Vector2<double> r4r{ 1.0, 1.0 };
	Vector2<double> r5r{ 0.0, 0.0 };
	Vector2<double> r6r{ -1.3, -1.3 };
	Vector2<double> r7r{ -2.6, -2.6 };
	Vector2<double> r8r{ -3.5, -3.5 };
	Vector2<double> r9r{ -1.0, -1.0 };

	r1r = r1r.Rounded();
	r2r = r2r.Rounded();
	r3r = r3r.Rounded();
	r4r = r4r.Rounded();
	r5r = r5r.Rounded();
	r6r = r6r.Rounded();
	r7r = r7r.Rounded();
	r8r = r8r.Rounded();
	r9r = r9r.Rounded();

	PTGN_ASSERT(r1r.x == 1.0);
	PTGN_ASSERT(r2r.x == 3.0);
	PTGN_ASSERT(r3r.x == 4.0);
	PTGN_ASSERT(r4r.x == 1.0);
	PTGN_ASSERT(r5r.x == 0.0);
	PTGN_ASSERT(r6r.x == -1.0);
	PTGN_ASSERT(r7r.x == -3.0);
	PTGN_ASSERT(r8r.x == -4.0);
	PTGN_ASSERT(r9r.x == -1.0);

	PTGN_ASSERT(r1r.y == 1.0);
	PTGN_ASSERT(r2r.y == 3.0);
	PTGN_ASSERT(r3r.y == 4.0);
	PTGN_ASSERT(r4r.y == 1.0);
	PTGN_ASSERT(r5r.y == 0.0);
	PTGN_ASSERT(r6r.y == -1.0);
	PTGN_ASSERT(r7r.y == -3.0);
	PTGN_ASSERT(r8r.y == -4.0);
	PTGN_ASSERT(r9r.y == -1.0);

	// Angle() tests.

	Vector2<int> rot1{ 1, 0 };
	Vector2<int> rot2{ -1, 0 };
	Vector2<int> rot3{ 0, 1 };
	Vector2<int> rot4{ 0, -1 };
	Vector2<int> rot5{ 1, 1 };
	Vector2<int> rot6{ -1, -1 };

	PTGN_ASSERT(NearlyEqual(rot1.Angle<float>(), 0.0f));
	PTGN_ASSERT(NearlyEqual(rot2.Angle<float>(), 3.14159f));
	PTGN_ASSERT(NearlyEqual(rot3.Angle<float>(), 1.5708f));
	PTGN_ASSERT(NearlyEqual(rot5.Angle<float>(), 0.785398f));
	PTGN_ASSERT(NearlyEqual(rot4.Angle<float>(), -1.5708f));
	PTGN_ASSERT(NearlyEqual(rot6.Angle<float>(), -2.35619f));

	// Rotated() tests.

	Vector2<int> rotate_me{ 1, 0 };
	Vector2<int> rotated_90{ rotate_me.Rotated(1.5708f) };
	Vector2<int> rotated_180{ rotate_me.Rotated(3.14159f) };
	Vector2<int> rotated_270{ rotate_me.Rotated(-1.5708f) };
	Vector2<int> rotated_360{ rotate_me.Rotated(0.0f) };

	Vector2<double> drotated_90{ rotate_me.Rotated(1.5708f) };
	Vector2<double> drotated_180{ rotate_me.Rotated(3.14159f) };
	Vector2<double> drotated_270{ rotate_me.Rotated(-1.5708f) };
	Vector2<double> drotated_360{ rotate_me.Rotated(0.0f) };

	PTGN_ASSERT(rotated_90.x == 0);
	PTGN_ASSERT(rotated_90.y == 1);
	PTGN_ASSERT(rotated_180.x == -1);
	PTGN_ASSERT(rotated_180.y == 0);
	PTGN_ASSERT(rotated_270.x == 0);
	PTGN_ASSERT(rotated_270.y == -1);
	PTGN_ASSERT(rotated_360.x == 1);
	PTGN_ASSERT(rotated_360.y == 0);

	PTGN_ASSERT(NearlyEqual(drotated_90.x, 0.0));
	PTGN_ASSERT(NearlyEqual(drotated_90.y, 1.0));
	PTGN_ASSERT(NearlyEqual(drotated_180.x, -1.0));
	PTGN_ASSERT(NearlyEqual(drotated_180.y, 0.0));
	PTGN_ASSERT(NearlyEqual(drotated_270.x, 0.0));
	PTGN_ASSERT(NearlyEqual(drotated_270.y, -1.0));
	PTGN_ASSERT(NearlyEqual(drotated_360.x, 1.0));
	PTGN_ASSERT(NearlyEqual(drotated_360.y, 0.0));

	// IsZero() tests.

	Vector2<double> test1{ 0.0, 0.0 };
	Vector2<float> test2{ 0.0f, 0.0f };
	Vector2<int> test3{ 0, 0 };

	Vector2<double> test1a{ 1.0, 1.0 };
	Vector2<float> test2a{ 1.0f, 1.0f };
	Vector2<int> test3a{ 1, 1 };

	test1a *= 2.0;
	test2a *= 2.0f;
	test3a *= 2;
	test1a -= Vector2<double>{ 2.0, 2.0 };
	test2a -= Vector2<float>{ 2.0f, 2.0f };
	test3a -= Vector2<int>{ 2, 2 };

	PTGN_ASSERT(test1.IsZero());
	PTGN_ASSERT(test2.IsZero());
	PTGN_ASSERT(test3.IsZero());
	PTGN_ASSERT(test1a.IsZero());
	PTGN_ASSERT(test2a.IsZero());
	PTGN_ASSERT(test3a.IsZero());

	// [] access operator tests.

	Vector2<int> access1{ 56, -73 };
	Vector2<float> access2{ -51.0f, 72.0f };
	Vector2<double> access3{ 32.0, -54.0 };

	PTGN_ASSERT(access1[0] == 56);
	PTGN_ASSERT(access1[1] == -73);
	PTGN_ASSERT(access2[0] == -51.0f);
	PTGN_ASSERT(access2[1] == 72.0f);
	PTGN_ASSERT(access3[0] == 32.0);
	PTGN_ASSERT(access3[1] == -54.0);

	access1[0] -= 3;
	access1[1]	= -2;
	access2[0] *= 2.0f;
	access2[1] *= -3.0f;
	access3[0] /= 2.0;
	access3[1]	= 555.0;

	PTGN_ASSERT(access1[0] == 56 - 3);
	PTGN_ASSERT(access1[1] == -2);
	PTGN_ASSERT(access2[0] == -51.0f * 2.0f);
	PTGN_ASSERT(access2[1] == 72.0f * -3.0f);
	PTGN_ASSERT(access3[0] == 32.0 / 2.0);
	PTGN_ASSERT(access3[1] == 555.0);

	// access1[-2] = 3;   // PTGN_ASSERT error called for index out of range.
	// access1[-1] = 3;   // PTGN_ASSERT error called for index out of range.
	// access1[3] = 3;    // PTGN_ASSERT error called for index out of range.
	// access1[4] = 3;    // PTGN_ASSERT error called for index out of range.
	// access2[5] = 3.0f; // PTGN_ASSERT error called for index out of range.
	// access2[6] = 3.0f; // PTGN_ASSERT error called for index out of range.
	// access3[7] = 3.0;  // PTGN_ASSERT error called for index out of range.
	// access3[8] = 3.0;  // PTGN_ASSERT error called for index out of range.

	// TODO:
	// Add tests for all +, -, *, / operators, don't forget to check narrowing
	// issues. Add tests for Cross() Add tests for Normalized() Add tests for
	// Skewed() Add tests for Identity() Add tests for Clamped()

	PTGN_INFO("All Vector2 tests passed!");
	return true;
}
