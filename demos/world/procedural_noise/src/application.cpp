#include "core/app/application.h"

#include <algorithm>
#include <cstdint>

#include "core/app/window.h"
#include "core/assert.h"
#include "core/ecs/components/movement.h"
#include "core/input/input_handler.h"
#include "core/input/key.h"
#include "core/log.h"
#include "math/math_utils.h"
#include "math/noise.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "renderer/api/origin.h"
#include "renderer/renderer.h"
#include "world/scene/camera.h"
#include "world/scene/scene.h"
#include "world/scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int resolution{ 800, 800 };

class NoiseExampleScene : public Scene {
public:
	ValueNoise value_noise;
	PerlinNoise perlin_noise;
	SimplexNoise simplex_noise;
	FractalNoise fractal_noise;

	std::size_t divisions{ 10 };

	V2_int pixel_size{ 16, 16 };

	bool thresholding{ false };

	int type{ 0 };
	int types{ 4 };

	void Enter() override {
		Application::Get().render_.SetBackgroundColor(color::Magenta);
		Application::Get().window_.SetResizable();
		PTGN_ASSERT(type == 0 || type == 1 || type == 2 || type == 3);
	}

	void Update() override {
		if (input.KeyDown(Key::Left)) {
			type--;
			type = Mod(type, types);
		} else if (input.KeyDown(Key::Right)) {
			type++;
			type = Mod(type, types);
		}

		if (input.KeyDown(Key::T)) {
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
		if (input.KeyDown(Key::G)) {
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
			if (input.KeyDown(Key::R)) {
				fractal_noise.SetOctaves(fractal_noise.GetOctaves() + 1);
			}
			if (input.KeyDown(Key::F)) {
				fractal_noise.SetOctaves(std::clamp((int)fractal_noise.GetOctaves() - 1, 1, 1000));
			}
			if (input.KeyDown(Key::Y)) {
				fractal_noise.SetLacunarity(fractal_noise.GetLacunarity() + 0.1f);
			}
			if (input.KeyDown(Key::H)) {
				fractal_noise.SetLacunarity(
					std::clamp(fractal_noise.GetLacunarity() - 0.1f, 0.001f, 1000.0f)
				);
			}

			if (input.KeyDown(Key::U)) {
				fractal_noise.SetPersistence(fractal_noise.GetPersistence() + 0.05f);
			}
			if (input.KeyDown(Key::J)) {
				fractal_noise.SetPersistence(
					std::clamp(fractal_noise.GetPersistence() - 0.05f, 0.001f, 1000.f)
				);
			}
		}

		auto cap_divisions = [&]() {
			divisions = std::clamp((int)divisions, 1, 32);
		};

		if (input.KeyDown(Key::Q)) {
			divisions--;
			cap_divisions();
		}
		if (input.KeyDown(Key::E)) {
			divisions++;
			cap_divisions();
		}

		if (input.KeyDown(Key::Z)) {
			thresholding = !thresholding;
		}

		MoveWASD(camera, V2_float{ 200.0f * Application::Get().dt() });

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

		if (input.KeyDown(Key::P)) {
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
		Draw();
	}

	void Draw() {
		auto vertices{ camera.GetWorldVertices() };
		V2_int min{ Floor(vertices[0] / pixel_size) - V2_int{ 1 } };
		V2_int max{ Ceil(vertices[2] / pixel_size) + V2_int{ 1 } };

		PTGN_LOG("Min: ", min, ", Max: ", max);

		PTGN_ASSERT(min.x < max.x && min.y < max.y);

		for (int i{ min.x }; i <= max.x; i++) {
			for (int j{ min.y }; j <= max.y; j++) {
				V2_int p{ i, j };

				float noise_value{ 0.0f };

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

				Color color{ color::White };
				if (thresholding) {
					float opacity_range = 1.0f / static_cast<float>(divisions);

					auto range = static_cast<int>(noise_value / opacity_range);

					color.a = static_cast<std::uint8_t>(
						255.0f * static_cast<float>(range) * opacity_range
					);
				} else {
					float opacity = noise_value * 255.0f;
					color.a		  = static_cast<std::uint8_t>(opacity);
				}
				Application::Get().render_.DrawRect(
					p * pixel_size, pixel_size, color, -1.0f, Origin::Center
				);
			}
		}

		Application::Get().render_.DrawRect(
			(min * pixel_size + max * pixel_size) * 0.5f, (max - min) * pixel_size, color::Orange,
			3.0f, Origin::Center
		);

		Application::Get().render_.DrawRect(
			{}, V2_float{ 30.0f, 30.0f }, color::Red, -1.0f, Origin::TopLeft
		);
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	Application::Get().Init("NoiseExample: Arrow keys to swap noise type", resolution);
	Application::Get().scene_.Enter<NoiseExampleScene>("");
	return 0;
}