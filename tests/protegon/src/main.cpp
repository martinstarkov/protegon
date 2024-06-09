#include "tests/test_ecs.h"
#include "test_math.h"
#include "test_vector2.h"
#include "test_rng.h"
#include "test_shader.h"
#include "test_shapes.h"
#include "test_text.h"

#include "protegon/protegon.h"

using namespace ptgn;

class Tests : public Scene {
public:
    Tests() {
        TestECS();
        TestMath();
        TestRNG();
        TestVector2();
        TestText();
        TestShader();
        TestShapes();
    }
    void Update(float dt) final {
        game::Stop();
    }
};

int main(int c, char** v) {
    ptgn::game::Start<Tests>();
    return 0;
}