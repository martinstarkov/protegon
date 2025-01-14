#pragma once

#include <memory>
#include <new>
#include <string>
#include <vector>

#include "common.h"
#include "components/camera_shake.h"
#include "components/sprite.h"
#include "components/transform.h"
#include "core/game.h"
#include "core/window.h"
#include "ecs/ecs.h"
#include "event/input_handler.h"
#include "event/key.h"
#include "math/geometry/polygon.h"
#include "math/math.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/origin.h"
#include "renderer/renderer.h"
#include "renderer/texture.h"
#include "scene/camera.h"

struct TestCameraSwitching : public Test {
	OrthographicCamera camera0;
	OrthographicCamera camera1;
	OrthographicCamera camera2;
	OrthographicCamera camera3;
	OrthographicCamera camera4;

	int camera{ 0 };

	const int cameras{ 5 };

	void Init() override {
		camera = 0;

		camera0 = game.camera.Load("0");
		camera1 = game.camera.Load("1");
		camera2 = game.camera.Load("2");
		camera3 = game.camera.Load("3");
		camera4 = game.camera.Load("4");

		camera0.SetPosition(V2_float{ 0, 0 });
		camera1.SetPosition(V2_float{ ws.x, 0 });
		camera2.SetPosition(ws);
		camera3.SetPosition(V2_float{ 0, ws.y });
		camera4.SetPosition(center);

		game.camera.SetPrimary(std::to_string(camera));
	}

	void Update() override {
		if (game.input.KeyDown(Key::E)) {
			camera++;
			camera = Mod(camera, cameras);
			game.camera.SetPrimary(std::to_string(camera));
		}
		if (game.input.KeyDown(Key::Q)) {
			camera--;
			camera = Mod(camera, cameras);
			game.camera.SetPrimary(std::to_string(camera));
		}
	}

	void Draw() override {
		Rect{ center, ws * 0.5f }.Draw(color::DarkGreen);
	}
};

struct TestParallax : public Test {
	V2_float bg_pos;
	V2_float planet_b_pos;
	V2_float planet_s_pos;
	V2_float stars_pos;

	Texture background;
	Texture planet_b;
	Texture planet_s;
	Texture stars;

	float scale{ 2.0f };

	V2_float size;

	V2_float background_size;
	float bg_aspect_ratio{ 0.0f };

	void Init() override {
		background = { "resources/sprites/parallax/background.png" };
		planet_b   = { "resources/sprites/parallax/planet_b.png" };
		planet_s   = { "resources/sprites/parallax/planet_s.png" };
		stars	   = { "resources/sprites/parallax/stars.png" };

		bg_pos		 = game.window.GetCenter();
		planet_b_pos = game.window.GetCenter() - V2_float{ 200, 200 };
		planet_s_pos = game.window.GetCenter() + V2_float{ 200, 200 };
		stars_pos	 = game.window.GetCenter();

		size			= ws * scale;
		background_size = background.GetSize();
		bg_aspect_ratio = background_size.x / background_size.y;
	}

	void Update() override {
		auto camera{ game.camera.GetPrimary() };

		camera.SetSize(ws);

		float speed = 200.5f * dt;

		V2_float velocity;

		if (game.input.KeyPressed(Key::W)) {
			velocity.y = -speed;
		}
		if (game.input.KeyPressed(Key::S)) {
			velocity.y = +speed;
		}
		if (game.input.KeyPressed(Key::A)) {
			velocity.x = -speed;
		}
		if (game.input.KeyPressed(Key::D)) {
			velocity.x = +speed;
		}
		camera.Translate({ velocity.x, velocity.y, 0.0f });

		if (game.input.KeyDown(Key::R)) {
			camera.SetPosition({ center.x, center.y, 0.0f });
		}

		stars_pos	 += velocity / 6.0f;
		bg_pos		 += velocity / 4.0f;
		planet_s_pos += velocity / 3.0f;
		planet_b_pos += velocity / 2.0f;
	}

	void Draw() override {
		auto camera{ game.camera.GetPrimary() };
		V2_float pos = camera.GetPosition();

		camera.SetPosition({ 0.0f, 0.0f });

		background.Draw({ bg_pos, { size.x * bg_aspect_ratio, size.y } });
		stars.Draw({ stars_pos, { size.x * bg_aspect_ratio, size.y } });

		planet_b.Draw({ planet_b_pos, planet_b.GetSize() * scale });
		planet_s.Draw({ planet_s_pos, planet_s.GetSize() * scale });

		camera.SetPosition(pos);
	}
};

void TestCamera() {
	std::vector<std::shared_ptr<Test>> tests;

	tests.emplace_back(new TestParallax());

	AddTests(tests);
}