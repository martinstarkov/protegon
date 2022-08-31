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

	// TODO:
	// Add tests for Rounded()
	// Add tests for Rotated()
	// Add tests for Angle()
	// Add tests for all +, -, *, / operators.

	std::cout << "All Vector2 tests passed!" << std::endl;
	return true;
}