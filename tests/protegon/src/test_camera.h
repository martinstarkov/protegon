#pragma once

#include "common.h"
#include "protegon/circle.h"
#include "protegon/line.h"
#include "protegon/polygon.h"

using namespace ptgn;

struct TestCameraSwitching : public Test {
	OrthographicCamera camera0;
	OrthographicCamera camera1;
	OrthographicCamera camera2;
	OrthographicCamera camera3;
	OrthographicCamera camera4;

	int camera{ 0 };
	const int cameras{ 5 };

	void Init() {
		camera0 = game.camera.Load(0);
		camera1 = game.camera.Load(1);
		camera2 = game.camera.Load(2);
		camera3 = game.camera.Load(3);
		camera4 = game.camera.Load(4);

		camera0.SetPosition(V2_float{ 0, 0 });
		camera1.SetPosition(V2_float{ ws.x, 0 });
		camera2.SetPosition(ws);
		camera3.SetPosition(V2_float{ 0, ws.y });
		camera4.SetPosition(center);

		game.camera.SetPrimary(camera);
	}

	void Shutdown() final {
		game.camera.ResetPrimaryToWindow();
	}

	void Update() {
		if (game.input.KeyDown(Key::E)) {
			camera++;
			camera = Mod(camera, cameras);
			game.camera.SetPrimary(camera);
		}
		if (game.input.KeyDown(Key::Q)) {
			camera--;
			camera = Mod(camera, cameras);
			game.camera.SetPrimary(camera);
		}
	}

	void Draw() {
		game.renderer.DrawRectangleFilled(center, ws * 0.5f, color::DarkGreen);
	}
};

struct TestCameraMovement : public Test {
	void Init() {}

	void Update(float dt) {
		auto& camera{ game.camera.GetPrimary() };

		float speed = 200.5f * dt;

		V3_float velocity;

		// TODO: Add rotation and zoom.
		// TODO: Move this stuff into camera controller class.

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
			velocity.x = speed;
		}
		camera.Translate(velocity);

		if (game.input.KeyDown(Key::R)) {
			camera.SetPosition({ center.x, center.y, 0.0f });
		}

		// TODO: Add zoom.
		/*
		game.input.SetRelativeMouseMode(true);

		std::size_t font_key = 0;
		game.font.Load(font_key, "resources/fonts/retro_gaming.ttf", 30);

		M4_float projection = M4_float::Orthographic(0.0f, (float)game.window.size.x, 0.0f,
		(float)game.window.size.y);
		M4_float projection = M4_float::Perspective(DegToRad(45.0f),
		(float)game.window.size.x / (float)game.window.size.y, 0.1f, 100.0f); M4_float
		projection = M4_float::Perspective(DegToRad(camera.zoom), (float)game.window.size.x
		/ (float)game.window.size.y, 0.1f, 100.0f);
		model = M4_float::Rotate(model, DegToRad(-55.0f), 1.0f, 0.0f, 0.0f);
		view = M4_float::Translate(view, 0.0f, 0.0f, -3.0f);

		int scroll = game.input.MouseScroll();

		if (scroll != 0) {
			camera.Zoom(scroll);
		}
		if (game.input.KeyPressed(Key::X)) {
			camera.Move(CameraDirection::Down, dt);
		}
		if (game.input.KeyPressed(Key::SPACE)) {
			camera.Move(CameraDirection::Up, dt);
		}
		if (game.input.KeyPressed(Key::A)) {
			view = M4_float::Translate(view, -0.05f, 0.0f, 0.0f);
		}
		if (game.input.KeyPressed(Key::D)) {
			view = M4_float::Translate(view, 0.05f, 0.0f, 0.0f);
		}
		if (game.input.KeyPressed(Key::W)) {
			view = M4_float::Translate(view, 0.0f, 0.05f, 0.0f);
		}
		if (game.input.KeyPressed(Key::S)) {
			view = M4_float::Translate(view, 0.0f, -0.05f, 0.0f);
		}
		if (game.input.KeyPressed(Key::Q)) {
			model = M4_float::Rotate(model, DegToRad(5.0f), 0.0f, 1.0f, 0.0f);
		}
		if (game.input.KeyPressed(Key::E)) {
			model = M4_float::Rotate(model, DegToRad(-5.0f), 0.0f, 1.0f, 0.0f);
		}
		if (game.input.KeyPressed(Key::Z)) {
			model = M4_float::Rotate(model, DegToRad(5.0f), 1.0f, 0.0f, 0.0f);
		}
		if (game.input.KeyPressed(Key::C)) {
			model = M4_float::Rotate(model, DegToRad(-5.0f), 1.0f, 0.0f, 0.0f);
		}*/
	}

	void Shutdown() final {
		game.camera.ResetPrimaryToWindow();
	}

	void Draw() {
		game.renderer.DrawRectangleFilled(center, game.window.GetSize() * 0.5f, color::DarkRed);
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

	void Init() {
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

	void Shutdown() final {
		game.camera.ResetPrimaryToWindow();
	}

	void Update(float dt) {
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

	void Draw() {
		auto& camera{ game.camera.GetPrimary() };
		V3_float pos = camera.GetPosition();

		camera.SetPosition({ 0.0f, 0.0f, 0.0f });

		game.renderer.DrawTexture(background, bg_pos, { size.x * bg_aspect_ratio, size.y });
		game.renderer.DrawTexture(stars, stars_pos, { size.x * bg_aspect_ratio, size.y });

		game.renderer.DrawTexture(planet_b, planet_b_pos, planet_b.GetSize() * scale);
		game.renderer.DrawTexture(planet_s, planet_s_pos, planet_s.GetSize() * scale);

		camera.SetPosition(pos);
	}
};

void TestCamera() {
	std::vector<std::shared_ptr<Test>> camera_tests;

	camera_tests.emplace_back(new TestCameraSwitching());
	camera_tests.emplace_back(new TestCameraMovement());
	camera_tests.emplace_back(new TestParallax());

	AddTests(camera_tests);
}