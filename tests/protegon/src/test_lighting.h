#pragma once

#include <memory>
#include <string>
#include <vector>

#include "common.h"
#include "core/game.h"
#include "event/input_handler.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/gl_renderer.h"
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
	LightEngine() = default;

	LightEngine(const V2_float& size, const Color& color) : size_{ size }, color_{ color } {
		shader = Shader(
			ShaderSource{
#include PTGN_SHADER_PATH(screen_default.vert)
			},
			ShaderSource{
#include PTGN_SHADER_PATH(lighting.frag)
			}
		);
	}

	Shader shader;
	V2_float size_;
	Color color_;
};

struct TLight {
	TLight(const V2_float& pos, const Color& color) : pos{ pos } {
		auto n		  = color.Normalized();
		this->color.x = n.x;
		this->color.y = n.y;
		this->color.z = n.z;
	}

	V2_float pos;
	V3_float color;
	float intensity{ 10.0f };
};

struct TestMouseLight : public Test {
	LightEngine light_engine;

	Texture test{ "resources/sprites/test1.jpg" };

	// RenderTexture blur{ ScreenShader::Blur };
	bool draw_blur{ true };
	std::vector<std::pair<TLight, int>> lights;
	const float speed{ 300.0f };

	void Shutdown() override {
		game.draw.SetTarget();
		game.draw.SetClearColor(color::White);
	}

	void Init() override {
		game.window.SetSize({ 800, 800 });
		game.draw.SetClearColor(color::Transparent);

		light_engine = { V2_float{ 800, 800 }, Color{ 32, 32, 32, 255 } };
		// light_engine.SetSoftShadow(true);
		light_engine.Load("mouse light", Light{ V2_float{ 400, 400 }, color::White, 5, true });

		lights.clear();
		lights.emplace_back(TLight{ V2_float{ 0, 0 }, color::White }, 0);
		lights.emplace_back(TLight{ V2_float{ 0, 0 }, color::Green }, 50);
		lights.emplace_back(TLight{ V2_float{ 0, 0 }, color::Blue }, 100);
		lights.emplace_back(TLight{ V2_float{ 0, 0 }, color::Magenta }, 150);
		lights.emplace_back(TLight{ V2_float{ 0, 0 }, color::Yellow }, 200);
		lights.emplace_back(TLight{ V2_float{ 0, 0 }, color::Cyan }, 250);
		lights.emplace_back(TLight{ V2_float{ 0, 0 }, color::Red }, 300);
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
		for (auto& [l, r] : lights) {
			l.pos.x = (float)r * std::sin(frequency * t) + c.x;
			l.pos.y = (float)r * std::cos(frequency * t) + c.y;
		}
	}

	void Draw() override {
		auto& shader = light_engine.shader;

		for (const auto& [l, r] : lights) {
			shader.Bind();
			shader.SetUniform("u_LightPos", l.pos);
			shader.SetUniform("u_LightColor", l.color);
			shader.SetUniform("u_LightIntensity", l.intensity);
			game.draw.Shader(shader);
		}
		game.draw.Rect({ 100, 100 }, { 100, 100 }, color::Blue, Origin::TopLeft);
		game.draw.Texture(test, game.window.GetSize() / 2, test.GetSize());
	}
};

void TestLighting() {
	std::vector<std::shared_ptr<Test>> tests;

	tests.emplace_back(new TestMouseLight());

	AddTests(tests);
}