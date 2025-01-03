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

struct TestCameraControls : public Test {
	Texture texture{ "resources/sprites/test1.jpg" };
	Texture ui_texture{ "resources/sprites/ui.jpg" };

	const float pan_speed	   = 200.0f;
	const float rotation_speed = 1.0f;
	const float zoom_speed{ 0.4f };

	void Init() override {
		auto camera2{ game.camera.GetPrimary() };
		camera2.SetPosition({ 0, 0 });
	}

	void Update() override {
		auto camera{ game.camera.GetPrimary() };

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

		// camera.PrintInfo();
	}

	void Draw() override {
		texture.Draw({ center, texture.GetSize() });
		ui_texture.Draw({ { 0, 0 }, ui_texture.GetSize(), Origin::TopLeft }, {}, { 1 });
		ui_texture.Draw({ { 0, 0 }, 3 * ui_texture.GetSize(), Origin::Center }, {}, { 2 });
	}
};

struct TestCameraBounds : public TestCameraControls {
	const float bound_width{ 3.0f };

	void Init() override {
		TestCameraControls::Init();

		auto camera{ game.camera.GetPrimary() };

		Rect bounds{ {}, { 800, 800 }, Origin::TopLeft };

		camera.SetBounds(bounds);
	}

	void Draw() override {
		TestCameraControls::Draw();
		auto camera{ game.camera.GetPrimary() };
		const auto& bounds{ camera.GetBounds() };
		bounds.Draw(color::Red, bound_width);
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

struct TestCameraShake : public Test {
	Texture texture{ "resources/sprites/test1.jpg" };

	TestCameraShake() {}

	ecs::Manager manager;
	ecs::Entity player;

	float speed{ 50.0f };

	void Init() override {
		manager.Reset();

		player = manager.CreateEntity();

		player.Add<Transform>(V2_float{ 60.0f, 60.0f });
		player.Add<CameraShake>();

		manager.Refresh();
	}

	void Shutdown() override {}

	void Update() override {
		auto& cam_shake = player.Get<CameraShake>();
		if (game.input.KeyDown(Key::R)) {
			cam_shake.Reset();
		}
		if (game.input.KeyDown(Key::T)) {
			cam_shake.Induce(0.1f);
		}
		if (game.input.KeyDown(Key::Y)) {
			cam_shake.Induce(0.15f);
		}
		if (game.input.KeyDown(Key::U)) {
			cam_shake.Induce(0.25f);
		}
		if (game.input.KeyDown(Key::I)) {
			cam_shake.Induce(0.5f);
		}
		if (game.input.KeyDown(Key::O)) {
			cam_shake.Induce(0.75f);
		}
		if (game.input.KeyDown(Key::P)) {
			cam_shake.Induce(1.0f);
		}

		cam_shake.Update();

		auto& p = player.Get<Transform>().position;
		if (game.input.KeyPressed(Key::W)) {
			p.y -= speed * dt;
		}
		if (game.input.KeyPressed(Key::S)) {
			p.y += speed * dt;
		}
		if (game.input.KeyPressed(Key::A)) {
			p.x -= speed * dt;
		}
		if (game.input.KeyPressed(Key::D)) {
			p.x += speed * dt;
		}

		auto cam = game.camera.GetPrimary();
		cam.SetPosition(p + cam_shake.local_position);
		cam.SetRotation(cam_shake.local_rotation);
	}

	void Draw() override {
		texture.Draw({ {}, texture.GetSize() });
		DrawRect(
			player, { player.Get<Transform>().position, V2_float{ 30.0f, 30.0f }, Origin::Center }
		);
		Rect{ { 0, 0 }, { 50.0f, 50.0f }, Origin::TopLeft }.Draw(color::Orange);
	}
};

void TestCamera() {
	std::vector<std::shared_ptr<Test>> tests;

	tests.emplace_back(new TestCameraControls());
	tests.emplace_back(new TestCameraShake());
	tests.emplace_back(new TestCameraBounds());
	tests.emplace_back(new TestCameraSwitching());
	tests.emplace_back(new TestParallax());

	AddTests(tests);
}