#pragma once

#include <cmath>
#include <memory>
#include <vector>

#include "common.h"
#include "core/game.h"
#include "core/manager.h"
#include "math/vector2.h"
#include "math/vector3.h"
#include "renderer/color.h"
#include "renderer/origin.h"
#include "renderer/shader.h"
#include "renderer/texture.h"

class Light {
public:
	Light() = default;

	Light(const V2_float& position, const Color& color, float intensity = 10.0f) :
		position{ position }, intensity{ intensity }, color_rgba_{ color } {
		auto n		  = color_rgba_.Normalized();
		this->color.x = n.x;
		this->color.y = n.y;
		this->color.z = n.z;
	}

	V2_float position;
	V3_float color;
	float intensity{ 10.0f };

private:
	Color color_rgba_;
};

class LightEngine : public MapManager<Light> {
public:
	LightEngine() {
		shader_ = Shader(
			ShaderSource{
#include PTGN_SHADER_PATH(screen_default.vert)
			},
			ShaderSource{
#include PTGN_SHADER_PATH(lighting.frag)
			}
		);
	}

	void Draw() {
		ForEachValue([&](const auto& light) {
			shader_.Bind();
			shader_.SetUniform("u_LightPos", light.position);
			shader_.SetUniform("u_LightColor", light.color);
			shader_.SetUniform("u_LightIntensity", light.intensity);
			game.draw.Shader(shader_, {}, {}, Origin::Center, BlendMode::Add);
			game.draw.Flush();
		});
		// game.draw.Shader(ScreenShader::Blur, BlendMode::Add);
	}

private:
	Shader shader_;
};

struct TestMouseLight : public Test {
	LightEngine light_engine;

	Texture test{ "resources/sprites/test1.jpg" };

	// RenderTexture blur{ ScreenShader::Blur };
	bool draw_blur{ true };
	const float speed{ 300.0f };

	void Shutdown() override {
		game.draw.SetTarget();
		game.draw.SetClearColor(color::White);
	}

	std::unordered_map<LightEngine::InternalKey, float> radii;

	void Init() override {
		game.window.SetSize({ 800, 800 });
		game.draw.SetClearColor(color::Transparent);

		light_engine = {};
		// light_engine.SetSoftShadow(true);
		light_engine.Load(0, Light{ V2_float{ 0, 0 }, color::White });
		light_engine.Load(1, Light{ V2_float{ 0, 0 }, color::Green });
		light_engine.Load(2, Light{ V2_float{ 0, 0 }, color::Blue });
		light_engine.Load(3, Light{ V2_float{ 0, 0 }, color::Magenta });
		light_engine.Load(4, Light{ V2_float{ 0, 0 }, color::Yellow });
		light_engine.Load(5, Light{ V2_float{ 0, 0 }, color::Cyan });
		light_engine.Load(6, Light{ V2_float{ 0, 0 }, color::Red });

		radii.clear();
		radii.reserve(light_engine.Size());
		float i{ 0.0f };
		light_engine.ForEachKey([&](auto key) {
			radii.emplace(key, i * 50.0f);
			i++;
		});
	}

	void Update() override {
		/*if (game.input.KeyPressed(Key::A)) {
			blur.camera.Translate({ speed * dt, 0.0f });
		}
		if (game.input.KeyPressed(Key::D)) {
			blur.camera.Translate({ -speed * dt, 0.0f });
		}
		if (game.input.KeyPressed(Key::W)) {
			blur.camera.Translate({ 0.0f, speed * dt });
		}
		if (game.input.KeyPressed(Key::S)) {
			blur.camera.Translate({ 0.0f, -speed * dt });
		}
		if (game.input.KeyDown(Key::B)) {
			draw_blur = !draw_blur;
		}*/

		auto t		   = game.time();
		auto frequency = 0.001f;
		V2_float c	   = game.window.GetSize() / 2.0f;

		light_engine.ForEachKeyValue([&](auto key, auto& light) {
			auto it = radii.find(key);
			PTGN_ASSERT(it != radii.end());
			float r{ it->second };
			light.position.x = r * std::sin(frequency * t) + c.x;
			light.position.y = r * std::cos(frequency * t) + c.y;
		});
	}

	void Draw() override {
		game.draw.Rect({ 100, 100 }, { 100, 100 }, color::Blue, Origin::TopLeft);
		game.draw.Texture(test, game.window.GetSize() / 2, test.GetSize());

		light_engine.Draw();
	}
};

void TestLighting() {
	std::vector<std::shared_ptr<Test>> tests;

	tests.emplace_back(new TestMouseLight());

	AddTests(tests);
}