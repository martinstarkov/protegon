#include <iostream>

#include "../tests/test_ecs.h"
#include "protegon/protegon.h"
#include "test_events.h"
#include "test_math.h"
#include "test_matrix4.h"
#include "test_rng.h"
#include "test_shader.h"
#include "test_shapes.h"
#include "test_text.h"
#include "test_vector2.h"

using namespace ptgn;

class Tests : public Scene {
public:
	Tests() {
		TestMatrix4();
		TestECS();
		TestMath();
		TestRNG();
		TestVector2();

		TestShader();
		TestEvents();
		TestText();
		TestShapes();
		game.Stop();
	}

	void Update() final {}
};

int main() {
	game.Start<Tests>();
	return 0;
}