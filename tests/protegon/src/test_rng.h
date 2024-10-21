#pragma once

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <memory>
#include <new>
#include <ostream>
#include <vector>

#include "common.h"
#include "core/game.h"
#include "core/window.h"
#include "event/input_handler.h"
#include "event/key.h"
#include "math/noise.h"
#include "math/rng.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/origin.h"
#include "renderer/renderer.h"
#include "utility/debug.h"
#include "utility/log.h"

struct TestNoise : public Test {
	ValueNoise value_noise;
	PerlinNoise perlin_noise;
	SimplexNoise simplex_noise;
	FractalNoise fractal_noise;

	std::size_t divisions{ 10 };

	V2_int pixel_size;

	bool thresholding{ false };

	int type{ 0 };

	explicit TestNoise(int type) : type{ type } {
		PTGN_ASSERT(type == 0 || type == 1 || type == 2 || type == 3);
	};

	void Shutdown() override {
		game.window.SetSetting(WindowSetting::Windowed);
	}

	void Init() override {
		game.window.Center();
		// game.window.SetSetting(WindowSetting::Fullscreen);
		ws		   = game.window.GetSize();
		pixel_size = { 8, 8 };

		if (type == 0) {
			PTGN_LOG("TEST: Fractal noise");
		} else if (type == 1) {
			PTGN_LOG("TEST: Perlin noise");
		} else if (type == 2) {
			PTGN_LOG("TEST: Simplex noise");
		} else if (type == 3) {
			PTGN_LOG("TEST: Value noise");
		}
	}

	void Update() override {
		if (game.input.KeyDown(Key::T)) {
			if (type == 0) {
				fractal_noise.SetFrequency(fractal_noise.GetFrequency() + 0.01f);
			} else if (type == 1) {
				perlin_noise.SetFrequency(perlin_noise.GetFrequency() + 0.01f);
			} else if (type == 2) {
				simplex_noise.SetFrequency(simplex_noise.GetFrequency() + 0.01f);
			} else if (type == 3) {
				value_noise.SetFrequency(value_noise.GetFrequency() + 0.01f);
			}
		}
		if (game.input.KeyDown(Key::G)) {
			if (type == 0) {
				fractal_noise.SetFrequency(fractal_noise.GetFrequency() - 0.01f);
			} else if (type == 1) {
				perlin_noise.SetFrequency(perlin_noise.GetFrequency() - 0.01f);
			} else if (type == 2) {
				simplex_noise.SetFrequency(simplex_noise.GetFrequency() - 0.01f);
			} else if (type == 3) {
				value_noise.SetFrequency(value_noise.GetFrequency() - 0.01f);
			}
		}

		if (type == 0) {
			if (game.input.KeyDown(Key::R)) {
				fractal_noise.SetOctaves(fractal_noise.GetOctaves() + 1);
			}
			if (game.input.KeyDown(Key::F)) {
				fractal_noise.SetOctaves(fractal_noise.GetOctaves() - 1);
			}
			if (game.input.KeyDown(Key::Y)) {
				fractal_noise.SetLacunarity(fractal_noise.GetLacunarity() + 0.1f);
			}
			if (game.input.KeyDown(Key::H)) {
				fractal_noise.SetLacunarity(fractal_noise.GetLacunarity() - 0.1f);
			}

			if (game.input.KeyDown(Key::U)) {
				fractal_noise.SetPersistence(fractal_noise.GetPersistence() + 0.05f);
			}
			if (game.input.KeyDown(Key::J)) {
				fractal_noise.SetPersistence(fractal_noise.GetPersistence() - 0.05f);
			}
		}

		auto cap_divisions = [&]() {
			divisions = std::clamp((int)divisions, 1, 32);
		};

		if (game.input.KeyDown(Key::Q)) {
			divisions--;
			cap_divisions();
		}
		if (game.input.KeyDown(Key::E)) {
			divisions++;
			cap_divisions();
		}

		if (game.input.KeyDown(Key::Z)) {
			thresholding = !thresholding;
		}

		auto& camera{ game.camera.GetPrimary() };

		const float pan_speed{ 200.0f };

		if (game.input.KeyPressed(Key::W)) {
			camera.Translate({ 0, -pan_speed * dt });
		}
		if (game.input.KeyPressed(Key::S)) {
			camera.Translate({ 0, pan_speed * dt });
		}
		if (game.input.KeyPressed(Key::A)) {
			camera.Translate({ -pan_speed * dt, 0 });
		}
		if (game.input.KeyPressed(Key::D)) {
			camera.Translate({ pan_speed * dt, 0 });
		}

		// Clamp fractal noise parameters.

		if (type == 0) {
			fractal_noise.SetOctaves(std::clamp((int)fractal_noise.GetOctaves(), 1, 15));
			fractal_noise.SetFrequency(std::clamp(fractal_noise.GetFrequency(), 0.005f, 1.0f));
			fractal_noise.SetLacunarity(std::clamp(fractal_noise.GetLacunarity(), 0.01f, 5.0f));
			fractal_noise.SetPersistence(std::clamp(fractal_noise.GetPersistence(), 0.01f, 3.0f));
		} else if (type == 1) {
			perlin_noise.SetFrequency(std::clamp(perlin_noise.GetFrequency(), 0.005f, 1.0f));
		} else if (type == 2) {
			simplex_noise.SetFrequency(std::clamp(simplex_noise.GetFrequency(), 0.005f, 1.0f));
		} else if (type == 3) {
			value_noise.SetFrequency(std::clamp(value_noise.GetFrequency(), 0.005f, 1.0f));
		}

		if (game.input.KeyDown(Key::P)) {
			PTGN_LOG("--------------------------------");
			if (type == 0) {
				PTGN_LOG("octaves: ", fractal_noise.GetOctaves());
				PTGN_LOG("frequency: ", fractal_noise.GetFrequency());
				PTGN_LOG("lacunarity: ", fractal_noise.GetLacunarity());
				PTGN_LOG("persistence: ", fractal_noise.GetPersistence());
			} else if (type == 1) {
				PTGN_LOG("frequency: ", perlin_noise.GetFrequency());
			} else if (type == 2) {
				PTGN_LOG("frequency: ", simplex_noise.GetFrequency());
			} else if (type == 3) {
				PTGN_LOG("frequency: ", value_noise.GetFrequency());
			}
			PTGN_LOG("divisions: ", divisions);
		}
	}

	void Draw() override {
		const auto& cam = game.camera.GetPrimary();

		auto rect = cam.GetRectangle();

		V2_int min{ rect.Min() / pixel_size - V2_int{ 1 } };
		V2_int max{ rect.Max() / pixel_size + V2_int{ 1 } };

		for (int i{ min.x }; i < max.x; i++) {
			for (int j{ min.y }; j < max.y; j++) {
				V2_int p{ i, j };

				float noise_value = 0.0f;

				if (type == 0) {
					noise_value = fractal_noise.Get((float)i, (float)j);
					/*
					// OR ALTERNATIVE:
					noise_value = FractalNoise::GetValue(
						(float)i, (float)j, 0, fractal_noise.GetFrequency(),
						fractal_noise.GetNoiseType(), fractal_noise.GetOctaves(),
						fractal_noise.GetLacunarity(), fractal_noise.GetPersistence(),
						fractal_noise.GetWeightedStrength()
					);*/
				} else if (type == 1) {
					noise_value = perlin_noise.Get((float)i, (float)j);
					/*
					// OR ALTERNATIVE:
					noise_value =
						PerlinNoise::GetValue((float)i, (float)j, 0, perlin_noise.GetFrequency());
					*/
				} else if (type == 2) {
					noise_value = simplex_noise.Get((float)i, (float)j);
					/*
					// OR ALTERNATIVE:
					noise_value =
						SimplexNoise::GetValue((float)i, (float)j, 0, simplex_noise.GetFrequency());
					*/
				} else if (type == 3) {
					noise_value = value_noise.Get((float)i, (float)j);
					/*
					// OR ALTERNATIVE:
					noise_value =
						ValueNoise::GetValue((float)i, (float)j, 0, value_noise.GetFrequency());
					*/
				}

				if (thresholding) {
					Color color = color::Black;

					float opacity_range = 1.0f / static_cast<float>(divisions);

					auto range = static_cast<int>(noise_value / opacity_range);

					color.a = static_cast<std::uint8_t>(
						255.0f * static_cast<float>(range) * opacity_range
					);

					game.draw.Rectangle(p * pixel_size, pixel_size, color, Origin::TopLeft);
				} else {
					Color color	  = color::Black;
					float opacity = noise_value * 255.0f;
					color.a		  = static_cast<std::uint8_t>(opacity);
					game.draw.Rectangle(p * pixel_size, pixel_size, color, Origin::TopLeft);
				}
			}
		}

		game.draw.Rectangle({}, { 30.0f, 30.0f }, color::Red, Origin::TopLeft);
	}
};

struct TestFractalNoise : public TestNoise {
	TestFractalNoise() : TestNoise{ 0 } {}
};

struct TestPerlinNoise : public TestNoise {
	TestPerlinNoise() : TestNoise{ 1 } {}
};

struct TestSimplexNoise : public TestNoise {
	TestSimplexNoise() : TestNoise{ 2 } {}
};

struct TestValueNoise : public TestNoise {
	TestValueNoise() : TestNoise{ 3 } {}
};

void TestNoise() {
	std::vector<std::shared_ptr<Test>> tests;

	tests.emplace_back(new TestFractalNoise());
	tests.emplace_back(new TestPerlinNoise());
	tests.emplace_back(new TestSimplexNoise());
	tests.emplace_back(new TestValueNoise());

	AddTests(tests);
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