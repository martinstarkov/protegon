#define SDL_MAIN_HANDLED

#include <iostream>

#include "../tests/test_ecs.h"
#include "common.h"
#include "protegon/protegon.h"
#include "test_camera.h"
#include "test_math.h"
#include "test_matrix4.h"
#include "test_renderer.h"
#include "test_rng.h"
#include "test_vector.h"

// #include "test_events.h"
// #include "test_text.h"
// #include "test_tween.h"

using namespace ptgn;

class Tests : public Scene {
public:
	Tests() {}

	void Init() final {
		game.renderer.SetClearColor(color::White);
		game.window.SetSize(V2_int{ 800, 800 });
		game.window.Show();

		TestMatrix4();
		TestECS();
		TestMath();
		TestVector2();

		TestCamera();

		TestRNG();

		TestRenderer();

		// TestTween();

		// TestText();
		// TestEvents();
		game.window.SetTitle("Update Loop");
	}

	void Update() final {
		game.Stop();
	}
};

int main() {
	game.Start<Tests>();
	return 0;
}