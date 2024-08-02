#define SDL_MAIN_HANDLED

#include <iostream>

#include "../tests/test_ecs.h"
#include "protegon/protegon.h"
#include "test_events.h"
#include "test_math.h"
#include "test_matrix4.h"
#include "test_renderer.h"
#include "test_rng.h"
#include "test_shapes.h"
#include "test_text.h"
#include "test_vector.h"

using namespace ptgn;

class Tests : public Scene {
public:
	Tests() {
		TestMatrix4();
		TestECS();
		TestMath();
		TestRNG();
		TestVector2();

		TestText();
		TestRenderer();
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