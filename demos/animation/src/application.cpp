#include "components/draw.h"
#include "components/transform.h"
#include "core/game.h"
#include "core/game_object.h"
#include "core/window.h"
#include "ecs/ecs.h"
#include "event/input_handler.h"
#include "event/mouse.h"
#include "math/geometry/circle.h"
#include "math/geometry/line.h"
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

	Tween test;
	ecs::Entity s1;

	void Enter() override {
		game.texture.Load("test", "resources/animation.png");

		s1 = CreateAnimation(manager, "test", 4, V2_float{ 16, 32 }, milliseconds{ 500 });
		s1.Add<Transform>(game.window.GetCenter(), 0.0f, scale);
		s1.Get<Tween>().Start();

		FadeIn(s1, milliseconds{ 5000 });
	}

	V2_float start_pos;

	GameObject c0;
	GameObject c1;

	void AddPan(const V2_float& pos) {
		V2_float start{ start_pos };
		c0 = manager.CreateEntity();
		c0.Add<Circle>(10.0f);
		c0.Add<Transform>(pos);
		c0.Add<Tint>(color::Red);
		c0.Add<Visible>();

		c1 = manager.CreateEntity();
		c1.Add<Line>(start_pos, pos);
		c1.Add<Transform>();
		c1.Add<LineWidth>(3.0f);
		c1.Add<Tint>(color::DarkGray);
		c1.Add<Visible>();

		start_pos = pos;
		// TODO: Clear on completion of the full thing.
		if (test.IsCompleted()) {
			test.Clear();
		}
		test.During(seconds{ 3 }).OnUpdate([=](float f) mutable {
			s1.Get<Transform>().position = Lerp(start, pos, f);
		});
		test.Start(false);
	}

	void Update() override {
		if (start_pos.IsZero()) {
			start_pos = s1.Get<Transform>().position;
		}
		if (game.input.MouseDown(Mouse::Left)) {
			auto mouse = game.input.GetMousePosition();
			AddPan(mouse);
		}
		test.Step(game.dt());
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("AnimationExample");
	game.scene.Enter<AnimationExample>("animation_example");
	return 0;
}