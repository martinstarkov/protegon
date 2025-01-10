#include <memory>
#include <vector>

#include "common.h"
#include "components/sprite.h"
#include "components/transform.h"
#include "ecs/ecs.h"
#include "math/vector2.h"
#include "renderer/origin.h"
#include "renderer/texture.h"
#include "utility/time.h"

class AnimationTest : public EntityTest {

	void Init() final {
	}

	void Draw() final {
	}
};

void TestAnimations() {
	std::vector<std::shared_ptr<Test>> tests;

	tests.emplace_back(new SpriteTest());
	tests.emplace_back(new AnimationTest());

	AddTests(tests);
}
