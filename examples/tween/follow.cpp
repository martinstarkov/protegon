#include <optional>
#include <vector>

#include "app/application.h"
#include "core/graphics/color.h"
#include "core/input/mouse.h"
#include "core/math/vector2.h"
#include "runtime/animation/follow_config.h"
#include "runtime/animation/tween_effect.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/render_context.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/tint.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_input.h"

using namespace ptgn;

struct FollowEffectScene : public Scene {
	Sprite mouse;

	Sprite entity1;
	Sprite entity2;
	Sprite entity3;
	Sprite entity4;
	Sprite entity5;

	TargetFollowConfig config1;
	TargetFollowConfig config2;
	TargetFollowConfig config3;
	PathFollowConfig config4;
	PathFollowConfig config5;

	std::vector<V2_float> waypoints;

	Sprite CreateFollower(const Color& color, const V2_float& start_position) {
		auto follower{ CreateSprite(*this, "smile", start_position) };
		SetTint(follower, color);
		return follower;
	}

	void OnEnter() override {
		SetBackgroundColor(color::DarkGray);

		ctx().asset.Load("smile", "assets/white_smile.png");

		mouse = Sprite{ CreateEntity() };
		SetPosition(mouse, {});

		entity1 = CreateFollower(color::Red, { -300, -300 });
		entity2 = CreateFollower(color::Green, { -200, -200 });
		entity3 = CreateFollower(color::Blue, { -100, -100 });
		entity4 = CreateFollower(color::Yellow, { 0, 0 });
		entity5 = CreateFollower(color::Magenta, { 100, 100 });

		// Target follow (snap).
		config1.lerp = { 1.0f, 1.0f };

		// Target follow (lerp).
		config2.lerp = { 0.6f, 0.6f };

		// Target follow (velocity).
		config3.move_mode = MoveMode::Velocity;

		// Path follow (lerp).
		config4.move_mode	  = MoveMode::Lerp;
		config4.lerp		  = { 0.6f, 0.6f };
		config4.loop_path	  = true;
		config4.stop_distance = 40.0f;

		// Path follow (velocity).
		config5.loop_path	  = true;
		config5.stop_distance = 40.0f;
		config5.move_mode	  = MoveMode::Velocity;

		V2_float game_size{ ctx().renderer.GetGameSize() };
		V2_float half{ game_size * 0.5f };
		V2_float center{ 0, 0 };

		waypoints = { -half, center, V2_float{ half.x, -half.y }, center,
					  half,	 center, V2_float{ -half.x, half.y }, center };

		Start();
	}

	void Start() {
		// config1.teleport_on_start = true;
		// config2.teleport_on_start = true;
		// config3.teleport_on_start = true;
		// config4.teleport_on_start = true;
		// config5.teleport_on_start = true;

		StartFollow(entity1, mouse, config1);
		StartFollow(entity2, mouse, config2);
		StartFollow(entity3, mouse, config3);
		StartFollow(entity4, waypoints, config4);
		StartFollow(entity5, waypoints, config5);
	}

	void Stop() {
		StopFollow(entity1);
		StopFollow(entity2);
		StopFollow(entity3);
		StopFollow(entity4);
		StopFollow(entity5);
	}

	void OnUpdate() override {
		SetPosition(mouse, ctx().input.GetMousePosition());
		if (ctx().input.MousePressed(Mouse::Left)) {
			Stop();
		} else if (ctx().input.MousePressed(Mouse::Right)) {
			Start();
		}
	}
};

int main(int, char**) {
	Application app{ "FollowEffectScene: Left/Right: Stop/Start Follow" };
	app.StartWith<FollowEffectScene>();
}