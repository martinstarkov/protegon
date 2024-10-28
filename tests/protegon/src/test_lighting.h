#pragma once

#include <memory>
#include <string>
#include <vector>

#include "common.h"
#include "core/game.h"
#include "math/vector2.h"
#include "renderer/batch.h"
#include "renderer/color.h"
#include "renderer/shader.h"

struct LightEngine {
	LightEngine(const V2_float& size, const Color& color) : size_{ size }, color_{ color } {
		blur_shader_x_ = Shader(
			ShaderSource{
#include PTGN_SHADER_PATH(color.vert)
			},
			ShaderSource{
#include PTGN_SHADER_PATH(blur_x.frag)
			}
		);
		blur_shader_y_ = Shader(
			ShaderSource{
#include PTGN_SHADER_PATH(color.vert)
			},
			ShaderSource{
#include PTGN_SHADER_PATH(blur_y.frag)
			}
		);

		light_shader_ = Shader(
			ShaderSource{
#include PTGN_SHADER_PATH(color.vert)
			},
			ShaderSource{
#include PTGN_SHADER_PATH(light_fs.frag)
			}
		);

		light_shader.setParameter("texture", sf::Shader::CurrentTexture);
		light_shader.setParameter("screenHeight", height);
	}

	Shader blur_shader_x_;
	Shader blur_shader_y_;
	Shader light_shader_;
	V2_float size_;
	Color color_;

	void SetSoftShadow(bool enabled) {
		// TODO: Implement.
	}

	LightKey AddSpotLight(
		const std::string& name, const V2_float& vector, const Color& color, float value,
		bool boolean
	) {
		// TODO: Implement.
	}

	void SetPosition(const LightKey& light_key, const V2_float& position) {
		// TODO: Implement.
	}

	void Draw() {
		// TODO: Implement.
	}
};

struct TestMouseLight : public Test {
	LightEngine light_engine{ V2_float{ 800, 800 }, Color{ 32, 32, 32, 255 } };

	LightKey mouse_light;

	void Shutdown() override {}

	void Init() override {
		light_engine.SetSoftShadow(true);
		mouse_light =
			light_engine.AddSpotLight("mouse light", V2_float{ 400, 400 }, color::White, 5, true);
	}

	void Update() override {
		light_engine.SetPosition(mouse_light, game.input.GetMousePosition());
	}

	void Draw() override {
		light_engine.Draw();
	}
};

void TestLighting() {
	std::vector<std::shared_ptr<Test>> tests;

	tests.emplace_back(new TestMouseLight());

	AddTests(tests);
}