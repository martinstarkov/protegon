#pragma once

#include <memory>
#include <string>
#include <vector>

#include "common.h"
#include "core/game.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/shader.h"

class Light {
public:
	Light() = default;

	Light(const V2_float& position, const Color& color, float intensity, bool dynamic) :
		position{ position }, color{ color }, intensity{ intensity }, dynamic{ dynamic } {}

	V2_float position;
	Color color{ color::Transparent };
	float intensity{ 0.0f };
	bool dynamic{ false };
};

class LightEngine : public MapManager<Light> {
public:
	LightEngine(const V2_float& size, const Color& color) : size_{ size }, color_{ color } {
		/*light_shader_ = Shader(
			ShaderSource{
#include PTGN_SHADER_PATH(color.vert)
			},
			ShaderSource{
#include PTGN_SHADER_PATH(light_fs.frag)
			}
		);

		light_shader.setParameter("texture", sf::Shader::CurrentTexture);
		light_shader.setParameter("screenHeight", height);
		*/
	}

	Shader light_shader_;
	V2_float size_;
	Color color_;

	void SetSoftShadow(bool enabled) {
		// TODO: Implement.
	}

	void SetPosition(const std::string_view& light_key, const V2_float& position) {
		// TODO: Implement.
	}

	void Draw() {
		// TODO: Implement.
	}
};

struct TestMouseLight : public Test {
	LightEngine light_engine{ V2_float{ 800, 800 }, Color{ 32, 32, 32, 255 } };

	void Shutdown() override {}

	void Init() override {
		// light_engine.SetSoftShadow(true);
		light_engine.Load("mouse light", Light{ V2_float{ 400, 400 }, color::White, 5, true });
	}

	void Update() override {
		light_engine.SetPosition("mouse light", game.input.GetMousePosition());
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