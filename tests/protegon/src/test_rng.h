#pragma once

#include "common.h"
#include "protegon/rng.h"
#include "utility/debug.h"

// TODO: Add tests for Gaussian.

using namespace ptgn;

struct TestFractalNoise : public Test {
	V2_float ws;

	std::size_t octaves{ 0 };
	float frequency{ 0.0f };
	float bias{ 0.0f };
	float persistence{ 0.0f };

	std::vector<float> noise_map;

	V2_float pos;

	ValueNoise noise{ 256, 0 };

	V2_float pixel_size;
	V2_float grid_size;
	V2_int grid_pos;

	TestFractalNoise(int fractal_preset = 0) {
		if (fractal_preset == 0) {
			octaves		= 5;
			frequency	= 0.03f;
			bias		= 2.4f;
			persistence = 0.7f;
		} else {
			octaves		= 5;
			frequency	= 0.02f;
			bias		= 1.8f;
			persistence = 0.35f;
		}
	}

	void Init() {
		ws	= game.window.GetSize();
		pos = {};

		pixel_size = { 8, 8 };
		grid_size  = ws / pixel_size;
		grid_pos   = pos / grid_size;

		noise_map = FractalNoise::Generate(
			noise, grid_pos, grid_size, octaves, frequency, bias, persistence
		);
	}

	void Update(float dt) {
		bool change{ false };

		if (game.input.KeyDown(Key::R)) {
			octaves++;
			change = true;
		}
		if (game.input.KeyDown(Key::F)) {
			octaves--;
			change = true;
		}

		if (game.input.KeyDown(Key::T)) {
			frequency += 0.01f;
			change	   = true;
		}
		if (game.input.KeyDown(Key::G)) {
			frequency -= 0.01f;
			change	   = true;
		}

		if (game.input.KeyDown(Key::Y)) {
			bias   += 0.2f;
			change	= true;
		}
		if (game.input.KeyDown(Key::H)) {
			bias   -= 0.2f;
			change	= true;
		}

		if (game.input.KeyDown(Key::U)) {
			persistence += 0.05f;
			change		 = true;
		}
		if (game.input.KeyDown(Key::J)) {
			persistence -= 0.05f;
			change		 = true;
		}

		const float pan_speed{ 100.0f };

		if (game.input.KeyPressed(Key::W)) {
			pos.y  -= pan_speed * dt;
			change	= true;
		}
		if (game.input.KeyPressed(Key::S)) {
			pos.y  += pan_speed * dt;
			change	= true;
		}
		if (game.input.KeyPressed(Key::A)) {
			pos.x  -= pan_speed * dt;
			change	= true;
		}
		if (game.input.KeyPressed(Key::D)) {
			pos.x  += pan_speed * dt;
			change	= true;
		}

		if (game.input.KeyDown(Key::P)) {
			PTGN_LOG("octaves = ", octaves, ";");
			PTGN_LOG("frequency = ", frequency, "f;");
			PTGN_LOG("bias = ", bias, ";");
			PTGN_LOG("persistence = ", persistence, "f;");
		}

		if (change) {
			octaves		= std::clamp((int)octaves, 1, 10);
			frequency	= std::clamp(frequency, 0.01f, 0.3f);
			bias		= std::clamp(bias, 0.2f, 4.0f);
			persistence = std::clamp(persistence, 0.05f, 1.0f);
			/*V2_int new_grid_pos = pos / ws * grid_size;
			if (new_grid_pos != grid_pos) {
				grid_pos = new_grid_pos;
				noise	 = ValueNoise(256, Hash(new_grid_pos));
			}*/
			noise_map = FractalNoise::Generate(
				noise, (V2_int)pos, grid_size, octaves, frequency, bias, persistence
			);
		}
	}

	void Draw() {
		for (std::size_t i{ 0 }; i < grid_size.x; i++) {
			for (std::size_t j{ 0 }; j < grid_size.y; j++) {
				V2_float p{ static_cast<float>(i), static_cast<float>(j) };
				Color color = color::Black;

				std::size_t index{ i + static_cast<std::size_t>(grid_size.x) * j };
				PTGN_ASSERT(index < noise_map.size());
				float opacity = noise_map[index] * 255.0f;
				color.a		  = static_cast<std::uint8_t>(opacity);

				game.renderer.DrawRectangleFilled(
					p * pixel_size, pixel_size, color, Origin::TopLeft
				);
			}
		}
	}
};

void TestNoise() {
	static int noise_test{ 0 };

	std::vector<std::shared_ptr<Test>> noise_tests;

	noise_tests.emplace_back(new TestFractalNoise());

	AddTests(noise_tests);
}

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

	TestNoise();

	PTGN_INFO("All RNG tests passed!");
}