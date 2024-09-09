#pragma once

#include "common.h"
#include "protegon/rng.h"
#include "utility/debug.h"

// TODO: Add tests for Gaussian.

using namespace ptgn;

struct TestFractalNoise : public Test {
	NoiseProperties properties;

	std::vector<float> noise_map;

	V2_float pos;

	ValueNoise noise{ 256, 0 };

	V2_int pixel_size;
	V2_int grid_size;

	TestFractalNoise(int fractal_preset = 0) {
		if (fractal_preset == 0) {
			properties.octaves	   = 5;
			properties.frequency   = 0.03f;
			properties.bias		   = 2.4f;
			properties.persistence = 0.7f;
		} else {
			properties.octaves	   = 5;
			properties.frequency   = 0.02f;
			properties.bias		   = 1.8f;
			properties.persistence = 0.35f;
		}
	}

	void Init() {
		pos = {};

		pixel_size = { 8, 8 };
		grid_size  = ws / pixel_size;

		noise_map = FractalNoise::Generate(noise, pos, grid_size, properties);
	}

	void Update(float dt) {
		static NoiseProperties prev_properties = properties;

		if (game.input.KeyDown(Key::R)) {
			properties.octaves++;
		}
		if (game.input.KeyDown(Key::F)) {
			properties.octaves--;
		}

		if (game.input.KeyDown(Key::T)) {
			properties.frequency += 0.01f;
		}
		if (game.input.KeyDown(Key::G)) {
			properties.frequency -= 0.01f;
		}

		if (game.input.KeyDown(Key::Y)) {
			properties.bias += 0.2f;
		}
		if (game.input.KeyDown(Key::H)) {
			properties.bias -= 0.2f;
		}

		if (game.input.KeyDown(Key::U)) {
			properties.persistence += 0.05f;
		}
		if (game.input.KeyDown(Key::J)) {
			properties.persistence -= 0.05f;
		}

		const float pan_speed{ 100.0f };

		bool change{ false };

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
			PTGN_LOG("octaves = ", properties.octaves, ";");
			PTGN_LOG("frequency = ", properties.frequency, "f;");
			PTGN_LOG("bias = ", properties.bias, "f;");
			PTGN_LOG("persistence = ", properties.persistence, "f;");
		}

		if (change || properties != prev_properties) {
			properties.octaves	   = std::clamp((int)properties.octaves, 1, 10);
			properties.frequency   = std::clamp(properties.frequency, 0.01f, 0.3f);
			properties.bias		   = std::clamp(properties.bias, 0.2f, 4.0f);
			properties.persistence = std::clamp(properties.persistence, 0.05f, 1.0f);
			noise_map = FractalNoise::Generate(noise, (V2_int)pos, grid_size, properties);
		}
	}

	void Draw() {
		for (int i{ 0 }; i < grid_size.x; i++) {
			for (int j{ 0 }; j < grid_size.y; j++) {
				V2_int p{ i, j };
				Color color = color::Black;

				int index{ i + grid_size.x * j };
				PTGN_ASSERT(index < (int)noise_map.size());
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