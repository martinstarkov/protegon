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
	Movement,
	Switching,
	Count
};

template <typename T, typename... Ts>
void TestCameraLoop(const T& function, const std::string& name, const Ts&... message) {
	TestLoop(
			camera_test_instructions, camera_test, (int)CameraTest::Count, camera_test_switch_keys,
			function, name, message...
	);
}

void TestCameraSwitching() {
	ws	   = game.window.GetSize();
	center = game.window.GetCenter();

	auto& camera1{ game.camera.Load(1) };
	auto& camera2{ game.camera.Load(2) };
	auto& camera3{ game.camera.Load(3) };
	auto& camera4{ game.camera.Load(4) };
	auto& camera5{ game.camera.Load(5) };

	camera1.SetPosition(V2_float{ 0, 0 });
	camera2.SetPosition(V2_float{ ws.x, 0 });
	camera3.SetPosition(ws);
	camera4.SetPosition(V2_float{ 0, ws.y });
	camera5.SetPosition(center);

	game.camera.SetPrimary(1);

	TestCameraLoop(
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

				auto& primary{ game.camera.GetPrimary() };

				PTGN_LOG("Pos: ", primary.GetPosition());
				PTGN_LOG("View: ", primary.GetView());
				PTGN_LOG("Proj: ", primary.GetProjection());
			},
			PTGN_FUNCTION_NAME()
	);
}

void TestCameraMovement() {
	TestCameraLoop(
			[&](float dt) {
				game.renderer.DrawRectangleFilled(
						center, game.window.GetSize() * 0.5f, color::DarkRed
				);

				auto& camera{ game.camera.GetPrimary() };

				float speed = 200.5f * dt;

				V3_float velocity;

				// TODO: Add rotation and zoom.
				// TODO: Move this stuff into camera controller class.

				if (game.input.KeyPressed(Key::W)) {
					velocity.y = +speed;
				}
				if (game.input.KeyPressed(Key::S)) {
					velocity.y = -speed;
				}
				if (game.input.KeyPressed(Key::A)) {
					velocity.x = +speed;
				}
				if (game.input.KeyPressed(Key::D)) {
					velocity.x = -speed;
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

void TestCamera() {
	PTGN_INFO("Starting camera tests...");

	game.window.SetSize({ 800, 800 });
	game.window.Show();
	ws	   = game.window.GetSize();
	center = game.window.GetCenter();
	game.renderer.SetClearColor(color::DarkGrey);

	game.LoopUntilQuit([&]() {
		switch (static_cast<CameraTest>(camera_test)) {
			case CameraTest::Movement:	TestCameraMovement(); break;
			case CameraTest::Switching: TestCameraSwitching(); break;
			default:					PTGN_ERROR("Failed to find a valid camera test");
		}
	});

	game.scene.GetTopActive().camera.ResetPrimaryToWindow();

	PTGN_INFO("All camera tests passed!");
}