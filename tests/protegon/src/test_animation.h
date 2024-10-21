#include <memory>
#include <vector>

#include "common.h"
#include "components/sprite.h"
#include "components/transform.h"
#include "core/game.h"
#include "ecs/ecs.h"
#include "math/vector2.h"
#include "renderer/origin.h"
#include "renderer/texture.h"
#include "utility/time.h"

class SpriteTest : public EntityTest {
	Texture texture{ "resources/sprites/animation.png" };

	V2_float scale{ 5.0f };

	void Init() final {
		entity.Add<Transform>(center, 0.0f, scale);
		const auto& s = entity.Add<Sprite>(
			texture, V2_float{ 0, 0 }, Origin::CenterBottom, V2_float{ 16, 32 }, V2_float{ 0, 0 }
		);
	}

	void Draw() final {
		const auto& t = entity.Get<Transform>();
		const auto& s = entity.Get<Sprite>();
		s.Draw(entity, t);
	}
};

class AnimationTest : public EntityTest {
	Texture texture{ "resources/sprites/animation.png" };

	V2_float scale{ 5.0f };

	void Init() final {
		entity.Add<Transform>(center, 0.0f, scale);
		auto& a = entity.Add<Animation>(
			texture, V2_float{ 16, 32 }, 4, milliseconds{ 500 }, V2_float{ 0, 0 },
			Origin::CenterBottom, 0, V2_float{ 0, 32 }
		);
		a.Start();
	}

	void Draw() final {
		const auto& t = entity.Get<Transform>();
		const auto& a = entity.Get<Animation>();
		a.Draw(entity, t);
	}
};

void TestAnimations() {
	std::vector<std::shared_ptr<Test>> tests;

	tests.emplace_back(new SpriteTest());
	tests.emplace_back(new AnimationTest());

	AddTests(tests);
}