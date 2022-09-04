#pragma once

#include "protegon/vector2.h"

#include <cassert>		 // assert
#include <iostream>		 // std::cout
#include <unordered_map> // std::unordered_map

using namespace ptgn;

bool TestVector2() {
	std::cout << "Starting Vector2 tests..." << std::endl;

	Vector2<int> t0{ 5, -2 };
	assert(t0.x == 5);
	assert(t0.y == -2);

	Vector2<int> t0b = -t0;
	assert(t0b.x == -5);
	assert(t0b.y == 2);
	assert(t0b == -t0);

	Vector2<int> t1{ 2, -1 };
	assert(t1.x == 2);
	assert(t1.y == -1);
	assert((t1 == Vector2<int>{ 2, -1 }));

	Vector2<double> t2{ 0, 1 };
	assert(t2.x == 0.0);
	assert(t2.y == 1.0);
	assert((t2 == Vector2<double>{ 0, 1 }));

	Vector2<double> t2b = -t2;
	assert(t2b.x == -0.0);
	assert(t2b.y == -1.0);
	assert(t2b == -t2);
	assert((t2b == Vector2<double>{ -0.0, -1.0 }));

	constexpr Vector2<double> t3{ -2.0, 3.0 };
	assert(t3.x == -2.0);
	assert(t3.y == 3.0);
	assert((t3 == Vector2<double>{ -2.0, 3.0 }));

	Vector2<float> t3a{ 0.0, 1.0 };
	Vector2<float> t3b = Vector2<float>{ 0.0, 1.0 };
	//Vector2<float> t3c = { 0.0, 1.0 }; // implicit narrowing
	Vector2<double> t3d{ 0.0f, 1.0f };
	Vector2<double> t3e = Vector2<double>{ 0.0f, 1.0f };
	Vector2<double> t3f = { 0.0f, 1.0f };

	Vector2<float> t4{ 0.0f, 1.0f };
	Vector2<double> t5{ 0.0, 1.0 };
	
	Vector2<float>  t6a = t4;
	Vector2<float>  t6b { t4 };
	Vector2<double> t6c = t4;
	Vector2<double> t6e { t4 };
    //Vector2<float>  t6f = t5; // implicit narrowing
	Vector2<float>  t6g { t5 };
	Vector2<double> t6h = t5;
	Vector2<double> t6i { t5 };

	double t7a = t2.Dot(t1);
	double t7b = t2.Dot(t2);
	int t7c = t1.Dot(t0);
	//int t7d = t1.Dot(t2); // implicit narrowing
	assert(t7a - (-1) < DBL_EPSILON);
	assert(t7b - (1) < DBL_EPSILON);
	assert(t7c == 12);

	// hashing test

	std::unordered_map<Vector2<int>, int> map1;
	map1.emplace(t1, 3);
	assert(map1.find(t1)->second == 3);
	map1.emplace(t0, 2);
	assert(map1.find(t0)->second == 2);

	std::unordered_map<Vector2<double>, int> map2;
	map2.emplace(t2, 1);
	assert(map2.find(t2)->second == 1);
	map2.emplace(t2b, 5);
	assert(map2.find(t2b)->second == 5);
	assert(map2.find(t1) == map2.end());

	// += | -= | *= | /= operator tests.

	Vector2<double> s1{ 1.0, 2.0 };
	Vector2<double> s2{ 3.0, 4.0 };
	s1 *= s2;
	assert(s1.x == 1.0 * 3.0);
	assert(s1.y == 2.0 * 4.0);
	assert(s2.x == 3.0);
	assert(s2.y == 4.0);

	Vector2<int> s3{ 3, 4 };
	Vector2<double> s4{ 5.0, 6.0 };
	//s3 *= s4; // implicit narrowing
	s4 *= s3;
	assert(s4.x == 3 * 5.0);
	assert(s4.y == 4 * 6.0);
	assert(s3.x == 3);
	assert(s3.y == 4);

	Vector2<double> s5{ 1.0, 2.0 };
	Vector2<double> s6{ 3.0, 4.0 };
	s5 /= s6;
	assert(s5.x == 1.0 / 3.0);
	assert(s5.y == 2.0 / 4.0);
	assert(s6.x == 3.0);
	assert(s6.y == 4.0);

	Vector2<int> s7{ 3, 4 };
	Vector2<double> s8{ 5.0, 6.0 };
	//s7 /= s8; // implicit narrowing
	s8 /= s7;
	assert(s8.x == 5.0 / 3);
	assert(s8.y == 6.0 / 4);
	assert(s7.x == 3);
	assert(s7.y == 4);

	Vector2<double> s9{ 1.0, 2.0 };
	Vector2<double> s10{ 3.0, 4.0 };
	s9 -= s10;
	assert(s9.x == 1.0 - 3.0);
	assert(s9.y == 2.0 - 4.0);
	assert(s10.x == 3.0);
	assert(s10.y == 4.0);

	Vector2<int> s11{ 3, 4 };
	Vector2<double> s12{ 5.0, 6.0 };
	//s11 -= s12; // implicit narrowing
	s12 -= s11;
	assert(s12.x == 5.0 - 3);
	assert(s12.y == 6.0 - 4);
	assert(s11.x == 3);
	assert(s11.y == 4);

	Vector2<double> s13{ 1.0, 2.0 };
	Vector2<double> s14{ 3.0, 4.0 };
	s13 += s14;
	assert(s13.x == 1.0 + 3.0);
	assert(s13.y == 2.0 + 4.0);
	assert(s14.x == 3.0);
	assert(s14.y == 4.0);

	Vector2<int> s15{ 3, 4 };
	Vector2<double> s16{ 5.0, 6.0 };
	//s15 += s16; // implicit narrowing
	s16 += s15;
	assert(s16.x == 5.0 + 3);
	assert(s16.y == 6.0 + 4);
	assert(s15.x == 3);
	assert(s15.y == 4);

	Vector2<int> p1{ 3, 4 };
	Vector2<double> p2{ 3.0, 4.0 };
	int p3{ 5 };
	double p4{ 6.0 };
	p1 *= p3;
	//p1 *= p4; // // implicit narrowing
	assert(p1.x == 3 * 5);
	assert(p1.y == 4 * 5);
	assert(p3 == 5);
	p2 *= p3;
	assert(p2.x == 3.0 * 5);
	assert(p2.y == 4.0 * 5);
	assert(p3 == 5);
	p2 *= p4;
	assert(p2.x == 3.0 * 5 * 6.0);
	assert(p2.y == 4.0 * 5 * 6.0);
	assert(p4 == 6.0);

	Vector2<int> q1{ 3, 4 };
	Vector2<double> q2{ 3.0, 4.0 };
	int q3{ 5 };
	double q4{ 6.0 };
	q1 /= q3;
	//q1 /= q4; // // implicit narrowing
	assert(q1.x == 3 / 5);
	assert(q1.y == 4 / 5);
	assert(q3 == 5);
	q2 /= q3;
	assert(q2.x == 3.0 / 5);
	assert(q2.y == 4.0 / 5);
	assert(q3 == 5);
	q2 /= q4;
	assert(q2.x == 3.0 / 5 / 6.0);
	assert(q2.y == 4.0 / 5 / 6.0);
	assert(q4 == 6.0);

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

	assert(r1r.x == 1.0);
	assert(r2r.x == 3.0);
	assert(r3r.x == 4.0);
	assert(r4r.x == 1.0);
	assert(r5r.x == 0.0);
	assert(r6r.x == -1.0);
	assert(r7r.x == -3.0);
	assert(r8r.x == -4.0);
	assert(r9r.x == -1.0);

	assert(r1r.y == 1.0);
	assert(r2r.y == 3.0);
	assert(r3r.y == 4.0);
	assert(r4r.y == 1.0);
	assert(r5r.y == 0.0);
	assert(r6r.y == -1.0);
	assert(r7r.y == -3.0);
	assert(r8r.y == -4.0);
	assert(r9r.y == -1.0);

	Vector2<int> rot1{  1,  0 };
	Vector2<int> rot2{ -1,  0 };
	Vector2<int> rot3{  0,  1 };
	Vector2<int> rot4{  0, -1 };
	Vector2<int> rot5{  1,  1 };
	Vector2<int> rot6{ -1, -1 };

	assert(NearlyEqual(rot1.Angle<float>(), 0.0f));
	assert(NearlyEqual(rot2.Angle<float>(), 3.14159f));
	assert(NearlyEqual(rot3.Angle<float>(), 1.5708f));
	assert(NearlyEqual(rot5.Angle<float>(), 0.785398f));
	assert(NearlyEqual(rot4.Angle<float>(), -1.5708f));
	assert(NearlyEqual(rot6.Angle<float>(), -2.35619f));

	Vector2<int> rotate_me{ 1, 0 };
	Vector2<int> rotated_90{ rotate_me.Rotated(1.5708f) };
	Vector2<int> rotated_180{ rotate_me.Rotated(3.14159f) };
	Vector2<int> rotated_270{ rotate_me.Rotated(-1.5708f) };
	Vector2<int> rotated_360{ rotate_me.Rotated(0.0f) };

	Vector2<double> drotated_90{ rotate_me.Rotated(1.5708f) };
	Vector2<double> drotated_180{ rotate_me.Rotated(3.14159f) };
	Vector2<double> drotated_270{ rotate_me.Rotated(-1.5708f) };
	Vector2<double> drotated_360{ rotate_me.Rotated(0.0f) };

	assert(rotated_90.x  ==  0);
	assert(rotated_90.y  ==  1);
	assert(rotated_180.x == -1);
	assert(rotated_180.y ==  0);
	assert(rotated_270.x ==  0);
	assert(rotated_270.y == -1);
	assert(rotated_360.x ==  1);
	assert(rotated_360.y ==  0);

	assert(NearlyEqual(drotated_90.x,   0.0));
	assert(NearlyEqual(drotated_90.y,   1.0));
	assert(NearlyEqual(drotated_180.x, -1.0));
	assert(NearlyEqual(drotated_180.y,  0.0));
	assert(NearlyEqual(drotated_270.x,  0.0));
	assert(NearlyEqual(drotated_270.y, -1.0));
	assert(NearlyEqual(drotated_360.x,  1.0));
	assert(NearlyEqual(drotated_360.y,  0.0));

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

	assert(test1.IsZero());
	assert(test2.IsZero());
	assert(test3.IsZero());
	assert(test1a.IsZero());
	assert(test2a.IsZero());
	assert(test3a.IsZero());



	// TODO:
	// Add tests for all +, -, *, / operators, don't forget to check narrowing issues.

	std::cout << "All Vector2 tests passed!" << std::endl;
	return true;
}