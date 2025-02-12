#include "components/draw.h"
#include "components/transform.h"
#include "core/game.h"
#include "core/window.h"
#include "ecs/ecs.h"
#include "math/vector2.h"
#include "renderer/texture.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "utility/time.h"
#include "utility/tween.h"

using namespace ptgn;

ecs::Entity FadeIn(ecs::Entity e, milliseconds duration) {
	auto fade_entity{ e.GetManager().CreateEntity() };
	fade_entity.Add<Tween>()
		.During(duration)
		.OnStart([=]() mutable {
			if (!e.Has<Tint>()) {
				e.Add<Tint>();
			}
		})
		.OnUpdate([=](float f) mutable {
			PTGN_ASSERT(
				e.Has<Tint>(), "Removed tint component from an entity which is currently fading"
			);
			auto& tint{ e.Get<Tint>() };
			tint = tint.WithAlpha(f);
		})
		.OnComplete([=]() mutable { fade_entity.Destroy(); })
		.Start();
	return fade_entity;
}

class AnimationExample : public Scene {
public:
	V2_float scale{ 5.0f };

	void Enter() override {
		game.texture.Load("test", "resources/animation.png");

		auto s1{ CreateAnimation(manager, "test", 4, V2_float{ 16, 32 }, milliseconds{ 500 }) };
		s1.Add<Transform>(game.window.GetCenter(), 0.0f, scale);
		s1.Get<Tween>().Start();

		FadeIn(s1, milliseconds{ 5000 });
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("AnimationExample");
	game.scene.Enter<AnimationExample>("animation_example");
	return 0;
}