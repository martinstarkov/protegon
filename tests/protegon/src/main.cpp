#include "../tests/test_ecs.h"
#include "protegon/protegon.h"
#include "test_math.h"
#include "test_rng.h"
#include "test_shader.h"
#include "test_shapes.h"
#include "test_text.h"
#include "test_vector2.h"

using namespace ptgn;

class Tests : public Scene {
public:
	Tests() {
		TestShader();
		TestECS();
		TestMath();
		TestRNG();
		TestVector2();
		TestText();
		TestShapes();
	}

	void Update() final {
		game::Stop();
	}
};

int main() {
	ptgn::game::Start<Tests>();
	// TestProgram();
	return 0;
}