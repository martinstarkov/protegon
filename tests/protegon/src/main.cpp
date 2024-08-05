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
#include "test_shapes.h"
#include "test_text.h"
#include "test_vector.h"

using namespace ptgn;

V2_float ws		= {};
V2_float center = {};

class Tests : public Scene {
public:
	Tests() {}

	void Init() final {
		TestMatrix4();
		TestECS();
		TestMath();
		TestRNG();
		TestVector2();

		TestCamera();
		TestRenderer();
		TestText();
		TestShapes();
		TestEvents();
		game.Stop();
	}

	void Update() final {}
};

int main() {
	game.Start<Tests>();
	game.Start<Tests>();
	return 0;
}