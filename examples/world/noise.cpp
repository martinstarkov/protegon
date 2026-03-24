#include "core/math/noise.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>

#include "app/application.h"
#include "core/assert.h"
#include "core/log.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/math_utils.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "platform/input/key.h"
#include "renderer/primitives/blend_mode.h"
#include "renderer/primitives/color.h"
#include "renderer/renderer.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/render_context.h"
#include "runtime/physics/movement.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_input.h"

using namespace ptgn;

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

	void OnEnter() override {
		ctx().renderer.SetBackgroundColor(color::Magenta);
		PTGN_ASSERT(type == 0 || type == 1 || type == 2 || type == 3);
	}

	void OnUpdate() override {
		if (ctx().input.KeyPressed(Key::Left)) {
			type--;
			type = Mod(type, types);
		} else if (ctx().input.KeyPressed(Key::Right)) {
			type++;
			type = Mod(type, types);
		}

		if (ctx().input.KeyPressed(Key::T)) {
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
		if (ctx().input.KeyPressed(Key::G)) {
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
			if (ctx().input.KeyPressed(Key::R)) {
				fractal_noise.SetOctaves(fractal_noise.GetOctaves() + 1);
			}
			if (ctx().input.KeyPressed(Key::F)) {
				fractal_noise.SetOctaves(std::clamp((int)fractal_noise.GetOctaves() - 1, 1, 1000));
			}
			if (ctx().input.KeyPressed(Key::Y)) {
				fractal_noise.SetLacunarity(fractal_noise.GetLacunarity() + 0.1f);
			}
			if (ctx().input.KeyPressed(Key::H)) {
				fractal_noise.SetLacunarity(
					std::clamp(fractal_noise.GetLacunarity() - 0.1f, 0.001f, 1000.0f)
				);
			}

			if (ctx().input.KeyPressed(Key::U)) {
				fractal_noise.SetPersistence(fractal_noise.GetPersistence() + 0.05f);
			}
			if (ctx().input.KeyPressed(Key::J)) {
				fractal_noise.SetPersistence(
					std::clamp(fractal_noise.GetPersistence() - 0.05f, 0.001f, 1000.f)
				);
			}
		}

		auto cap_divisions = [&]() {
			divisions = std::clamp((int)divisions, 1, 32);
		};

		if (ctx().input.KeyPressed(Key::Q)) {
			divisions--;
			cap_divisions();
		}
		if (ctx().input.KeyPressed(Key::E)) {
			divisions++;
			cap_divisions();
		}

		if (ctx().input.KeyPressed(Key::Z)) {
			thresholding = !thresholding;
		}

		constexpr V2_float speed{ 200.0f };
		float dt{ ctx().dt().count() };

		MoveWASD(ctx().camera, speed * dt);

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

		if (ctx().input.KeyPressed(Key::P)) {
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
		auto vertices{ ctx().camera.GetWorldVertices() };
		V2_int min{ FastFloor(vertices[0] / pixel_size) - V2_int{ 1 } };
		V2_int max{ FastCeil(vertices[2] / pixel_size) + V2_int{ 1 } };

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

				ctx().renderer.DrawShape(
					Rect{ pixel_size }, Transform{ p * pixel_size }, color, FillStyle::Solid(),
					Origin::Center, Depth{}, BlendMode::Blend
				);
			}
		}

		ctx().renderer.DrawShape(
			Rect{ (max - min) * pixel_size },
			Transform{ (min * pixel_size + max * pixel_size) * 0.5f }, color::Orange,
			FillStyle::Hollow(3.0f), Origin::Center, Depth{}, BlendMode::Blend
		);

		ctx().renderer.DrawShape(
			Rect{ 30, 30 }, Transform{}, color::Red, FillStyle::Solid(), Origin::TopLeft, Depth{},
			BlendMode::Blend
		);
	}
};

int main(int, char**) {
	Application app{ "NoiseExample: Arrow keys to swap noise type" };
	app.StartWith<NoiseExampleScene>();
}