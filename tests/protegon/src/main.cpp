#define SDL_MAIN_HANDLED

#include <iostream>

#include "../tests/test_ecs.h"
#include "common.h"
#include "protegon/protegon.h"
#include "test_camera.h"
#include "test_events.h"
#include "test_math.h"
#include "test_matrix4.h"
#include "test_renderer.h"
#include "test_rng.h"
#include "test_text.h"
#include "test_tween.h"
#include "test_vector.h"

using namespace ptgn;

V2_float ws;
V2_float center;

class Tests : public Scene {
public:
	Tests() {}

	void Init() final {
		game.window.SetSize({ 800, 800 });
		game.window.Show();
		ws = game.window.GetSize();
		game.renderer.SetClearColor(color::DarkRed);

		TestMatrix4();
		TestECS();
		TestMath();
		TestRNG();
		TestVector2();

		TestTween();

		TestRenderer();
		TestCamera();
		TestText();
		TestEvents();
		game.window.SetTitle("Update Loop");
	}

	void Update() final {
		game.renderer.DrawRectangleFilled(
			V2_float{ 0, 0 }, V2_float{ 50, 50 }, color::Blue, Origin::TopLeft
		);
		game.renderer.DrawRectangleFilled(
			V2_float{ ws.x, 0 }, V2_float{ 50, 50 }, color::Magenta, Origin::TopRight
		);
		game.renderer.DrawRectangleFilled(ws, V2_float{ 50, 50 }, color::Red, Origin::BottomRight);
		game.renderer.DrawRectangleFilled(
			V2_float{ 0, ws.y }, V2_float{ 50, 50 }, color::Orange, Origin::BottomLeft
		);
	}
};

int main() {
	game.Start<Tests>();
	return 0;
}