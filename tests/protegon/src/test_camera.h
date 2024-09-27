#pragma once

#include <memory>
#include <new>
#include <vector>

#include "common.h"
#include "core/window.h"
#include "event/input_handler.h"
#include "event/key.h"
#include "protegon/circle.h"
#include "protegon/color.h"
#include "protegon/game.h"
#include "protegon/line.h"
#include "protegon/math.h"
#include "protegon/polygon.h"
#include "protegon/texture.h"
#include "protegon/vector2.h"
#include "renderer/origin.h"
#include "renderer/renderer.h"
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
		game.draw.Rectangle(center, ws * 0.5f, color::DarkGreen);
	}
};

struct TestCameraControls : public Test {
	Texture texture{ "resources/sprites/test1.jpg" };

	const float pan_speed	   = 200.0f;
	const float rotation_speed = 1.0f;
	const float zoom_speed{ 0.4f };

	void Update() override {
		auto& camera{ game.camera.GetPrimary() };

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

		if (game.input.KeyPressed(Key::Z)) {
			camera.Yaw(rotation_speed * dt);
		}

		if (game.input.KeyPressed(Key::X)) {
			camera.Yaw(-rotation_speed * dt);
		}

		if (game.input.KeyPressed(Key::C)) {
			camera.Pitch(rotation_speed * dt);
		}

		if (game.input.KeyPressed(Key::V)) {
			camera.Pitch(-rotation_speed * dt);
		}

		if (game.input.KeyPressed(Key::B)) {
			camera.Roll(rotation_speed * dt);
		}

		if (game.input.KeyPressed(Key::N)) {
			camera.Roll(-rotation_speed * dt);
		}

		if (game.input.KeyPressed(Key::E)) {
			camera.Zoom(zoom_speed * dt);
		}
		if (game.input.KeyPressed(Key::Q)) {
			camera.Zoom(-zoom_speed * dt);
		}

		if (game.input.KeyDown(Key::R)) {
			camera.SetPosition({ center.x, center.y, 0.0f });
		}

		camera.PrintInfo();
	}

	void Draw() override {
		game.draw.Texture(texture, center, texture.GetSize());
	}
};

struct TestCameraBounds : public TestCameraControls {
	const float bound_width{ 3.0f };

	void Init() override {
		auto& camera{ game.camera.GetPrimary() };

		Rectangle<float> bounds{ {}, { 800, 800 }, Origin::TopLeft };

		camera.SetBounds(bounds);
	}

	void Draw() override {
		TestCameraControls::Draw();
		const auto& camera{ game.camera.GetPrimary() };
		const auto& bounds{ camera.GetBounds() };
		game.draw.Rectangle(bounds.pos, bounds.size, color::Red, bounds.origin, bound_width);
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
		auto& camera{ game.camera.GetPrimary() };

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
		auto& camera{ game.camera.GetPrimary() };
		V2_float pos = camera.GetPosition();

		camera.SetPosition({ 0.0f, 0.0f });

		game.draw.Texture(background, bg_pos, { size.x * bg_aspect_ratio, size.y });
		game.draw.Texture(stars, stars_pos, { size.x * bg_aspect_ratio, size.y });

		game.draw.Texture(planet_b, planet_b_pos, planet_b.GetSize() * scale);
		game.draw.Texture(planet_s, planet_s_pos, planet_s.GetSize() * scale);

		camera.SetPosition(pos);
	}
};

void TestCamera() {
	std::vector<std::shared_ptr<Test>> tests;

	tests.emplace_back(new TestCameraBounds());
	tests.emplace_back(new TestCameraControls());
	tests.emplace_back(new TestCameraSwitching());
	tests.emplace_back(new TestParallax());

	AddTests(tests);
}