#pragma once

#include "common.h"
#include "protegon/circle.h"
#include "protegon/line.h"
#include "protegon/polygon.h"

using namespace ptgn;

int camera_test = 0;
const std::vector<Key> camera_test_switch_keys{ Key::Q, Key::E };
const std::string camera_test_instructions{ "'Q' (cycle back); 'E' (cycle forward)" };

enum class CameraTest {
	Parallax,
	Switching,
	Movement,
	Count
};

void TestCameraSwitching(float dt) {
	ws	   = game.window.GetSize();
	center = game.window.GetCenter();

	auto get_camera = [](std::size_t key) {
		if (!game.camera.Has(key)) {
			return game.camera.Load(key);
		} else {
			return game.camera.Get(key);
		}
	};

	OrthographicCamera camera1{ get_camera(1) };
	OrthographicCamera camera2{ get_camera(2) };
	OrthographicCamera camera3{ get_camera(3) };
	OrthographicCamera camera4{ get_camera(4) };
	OrthographicCamera camera5{ get_camera(5) };

	camera1.SetPosition(V2_float{ 0, 0 });
	camera2.SetPosition(V2_float{ ws.x, 0 });
	camera3.SetPosition(ws);
	camera4.SetPosition(V2_float{ 0, ws.y });
	camera5.SetPosition(center);

	auto primary = game.camera.GetPrimary();
	if (primary != camera1 && primary != camera2 && primary != camera3 && primary != camera4 &&
		primary != camera5) {
		game.camera.SetPrimary(1);
	}

	TestCameraLoop(
		dt,
		[&](float dt) {
			game.renderer.DrawRectangleFilled(center, ws * 0.5f, color::DarkGreen);

			if (game.input.KeyDown(Key::K_1)) {
				game.camera.SetPrimary(1);
			}
			if (game.input.KeyDown(Key::K_2)) {
				game.camera.SetPrimary(2);
			}
			if (game.input.KeyDown(Key::K_3)) {
				game.camera.SetPrimary(3);
			}
			if (game.input.KeyDown(Key::K_4)) {
				game.camera.SetPrimary(4);
			}
			if (game.input.KeyDown(Key::K_5)) {
				game.camera.SetPrimary(5);
			}
		},
		PTGN_FUNCTION_NAME()
	);
}

void TestCameraMovement(float dt) {
	TestCameraLoop(
		dt,
		[&](float dt) {
			game.renderer.DrawRectangleFilled(center, game.window.GetSize() * 0.5f, color::DarkRed);

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
		},
		PTGN_FUNCTION_NAME()
	);

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

void TestParallax(float dt) {
	Texture background{ "resources/sprites/parallax/background.png" };
	Texture planet_b{ "resources/sprites/parallax/planet_b.png" };
	Texture planet_s{ "resources/sprites/parallax/planet_s.png" };
	Texture stars{ "resources/sprites/parallax/stars.png" };

	static V2_float bg_pos		 = game.window.GetCenter();
	static V2_float planet_b_pos = game.window.GetCenter() - V2_float{ 200, 200 };
	static V2_float planet_s_pos = game.window.GetCenter() + V2_float{ 200, 200 };
	static V2_float stars_pos	 = game.window.GetCenter();

	TestCameraLoop(
		dt,
		[&](float dt) {
			V2_float w = game.window.GetSize();

			V2_float background_size{ background.GetSize() };
			float bg_aspect_ratio{ background_size.x / background_size.y };

			float scale{ 2.0f };

			V2_float size{ w * scale };

			auto& camera{ game.camera.GetPrimary() };

			camera.SetSize(w);

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

			auto pos = camera.GetPosition();

			camera.SetPosition({ 0.0f, 0.0f, 0.0f });

			stars_pos	 += velocity / 6.0f;
			bg_pos		 += velocity / 4.0f;
			planet_s_pos += velocity / 3.0f;
			planet_b_pos += velocity / 2.0f;

			game.renderer.DrawTexture(background, bg_pos, { size.x * bg_aspect_ratio, size.y });
			game.renderer.DrawTexture(stars, stars_pos, { size.x * bg_aspect_ratio, size.y });

			game.renderer.DrawTexture(planet_b, planet_b_pos, planet_b.GetSize() * scale);
			game.renderer.DrawTexture(planet_s, planet_s_pos, planet_s.GetSize() * scale);

			camera.SetPosition(pos);
		},
		PTGN_FUNCTION_NAME()
	);
}

void TestCamera() {
	game.PushLoopFunction([&](float dt) {
		ws	   = game.window.GetSize();
		center = game.window.GetCenter();
		game.window.SetSize({ 800, 800 });
		game.renderer.SetClearColor(color::DarkGrey);
		static std::size_t count{ game.LoopFunctionCount() };
		switch (static_cast<CameraTest>(camera_test)) {
			case CameraTest::Parallax:	TestParallax(dt); break;
			case CameraTest::Movement:	TestCameraMovement(dt); break;
			case CameraTest::Switching: TestCameraSwitching(dt); break;
			default:					PTGN_ERROR("Failed to find a valid camera test");
		}
		if (count != game.LoopFunctionCount()) {
			game.scene.GetTopActive().camera.ResetPrimaryToWindow();
		}
	});
}