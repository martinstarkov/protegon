#pragma once

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <memory>
#include <new>
#include <ostream>
#include <vector>

#include "common.h"
#include "math/rng.h"
#include "utility/debug.h"
#include "utility/log.h"

void TestRNG() {
	PTGN_INFO("Starting RNG tests...");

	int test_amount = 100000;

	bool zero_found	 = false;
	bool one_found	 = false;
	bool two_found	 = false;
	bool three_found = false;
	bool four_found	 = false;
	bool five_found	 = false;
	bool six_found	 = false;

	RNG<int> r1; // seedless, default range: [0, 1], inclusive.

	for (auto i = 0; i < test_amount; ++i) {
		int value = r1();
		if (value == 0) {
			zero_found = true;
		}
		if (value == 1) {
			one_found = true;
		}
		if (value == 2) {
			two_found = true;
		}
		if (value == 3) {
			three_found = true;
		}
		if (value == 4) {
			four_found = true;
		}
		if (value == 5) {
			five_found = true;
		}
		if (value == 6) {
			six_found = true;
		}
	}

	PTGN_ASSERT(zero_found);
	PTGN_ASSERT(one_found);
	PTGN_ASSERT(!two_found);
	PTGN_ASSERT(!three_found);
	PTGN_ASSERT(!four_found);
	PTGN_ASSERT(!five_found);
	PTGN_ASSERT(!six_found);

	zero_found	= false;
	one_found	= false;
	two_found	= false;
	three_found = false;
	four_found	= false;
	five_found	= false;
	six_found	= false;

	RNG<int> r2{ 3 }; // seeded with #3, default range: [0, 1], inclusive.

	for (auto i = 0; i < test_amount; ++i) {
		int value = r2();
		if (value == 0) {
			zero_found = true;
		}
		if (value == 1) {
			one_found = true;
		}
		if (value == 2) {
			two_found = true;
		}
		if (value == 3) {
			three_found = true;
		}
		if (value == 4) {
			four_found = true;
		}
		if (value == 5) {
			five_found = true;
		}
		if (value == 6) {
			six_found = true;
		}
	}

	PTGN_ASSERT(zero_found);
	PTGN_ASSERT(one_found);
	PTGN_ASSERT(!two_found);
	PTGN_ASSERT(!three_found);
	PTGN_ASSERT(!four_found);
	PTGN_ASSERT(!five_found);
	PTGN_ASSERT(!six_found);

	zero_found	= false;
	one_found	= false;
	two_found	= false;
	three_found = false;
	four_found	= false;
	five_found	= false;
	six_found	= false;

	RNG<int> r3{ 3, 6 }; // seedless, custom range: [3, 6], inclusive.

	for (auto i = 0; i < test_amount; ++i) {
		int value = r3();
		if (value == 0) {
			zero_found = true;
		}
		if (value == 1) {
			one_found = true;
		}
		if (value == 2) {
			two_found = true;
		}
		if (value == 3) {
			three_found = true;
		}
		if (value == 4) {
			four_found = true;
		}
		if (value == 5) {
			five_found = true;
		}
		if (value == 6) {
			six_found = true;
		}
	}

	PTGN_ASSERT(!zero_found);
	PTGN_ASSERT(!one_found);
	PTGN_ASSERT(!two_found);
	PTGN_ASSERT(three_found);
	PTGN_ASSERT(four_found);
	PTGN_ASSERT(five_found);
	PTGN_ASSERT(six_found);

	zero_found	= false;
	one_found	= false;
	two_found	= false;
	three_found = false;
	four_found	= false;
	five_found	= false;
	six_found	= false;

	RNG<int> r4{ 1, 3, 6 }; // seeded with #1, custom range: [3, 6], inclusive.
	r4.SetSeed(3);			// seed changed to 3.

	for (auto i = 0; i < test_amount; ++i) {
		int value = r4();
		if (value == 0) {
			zero_found = true;
		}
		if (value == 1) {
			one_found = true;
		}
		if (value == 2) {
			two_found = true;
		}
		if (value == 3) {
			three_found = true;
		}
		if (value == 4) {
			four_found = true;
		}
		if (value == 5) {
			five_found = true;
		}
		if (value == 6) {
			six_found = true;
		}
	}

	PTGN_ASSERT(!zero_found);
	PTGN_ASSERT(!one_found);
	PTGN_ASSERT(!two_found);
	PTGN_ASSERT(three_found);
	PTGN_ASSERT(four_found);
	PTGN_ASSERT(five_found);
	PTGN_ASSERT(six_found);

	RNG<float> r5a{ 400.0f, 600.0f }; // seedless, custom range: [400.0f, 600.0f], inclusive.

	for (auto i = 0; i < test_amount; ++i) {
		float value = r5a();
		PTGN_ASSERT(value >= 400.0f);
		if (value > 600.0f) {
			std::cout << "value was: " << value;
		}
		PTGN_ASSERT(value <= 600.0f);
	}

	RNG<double> r5b{ -30.0, 60.0 }; // seedless, custom range: [-30.0, 60.0], inclusive.

	for (auto i = 0; i < test_amount; ++i) {
		double value = r5b();
		PTGN_ASSERT(value >= -30.0);
		if (value > 60.0) {
			std::cout << "value was: " << value;
		}
		PTGN_ASSERT(value <= 60.0);
	}

	RNG<std::size_t> r5c{ 0, 300 }; // seedless, custom range: [0, 300], inclusive.

	for (auto i = 0; i < test_amount; ++i) {
		std::size_t value = r5c();
		PTGN_ASSERT(value <= 300);
	}

	/*
	// std::uint8_t not supported by std::uniform_int_distribution.
	RNG<std::uint8_t> r5d{ 0, 255 };  // seedless, custom range: [0, 255],
	inclusive.

	for (auto i = 0; i < test_amount; ++i) {
		std::uint8_t value = r5d();
		PTGN_ASSERT(value >= 0);
		PTGN_ASSERT(value <= 255);
	}
	*/

	PTGN_INFO("All RNG tests passed!");
}