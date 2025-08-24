#include "components/sprite.h"
#include "components/transform.h"
#include "core/game.h"
#include "input/input_handler.h"
#include "math/vector2.h"
#include "renderer/renderer.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "tweens/tween_effects.h"

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

	Entity CreateFollower(const Color& color, const V2_float& start_position) {
		auto follower{ CreateSprite(*this, "smile", start_position) };
		SetTint(follower, color);
		return follower;
	}

	void Enter() override {
		LoadResource("smile", "resources/smile.png");

		mouse = CreateEntity();
		SetPosition(mouse, {});

		entity1 = CreateFollower(color::Red, { 100, 100 });
		entity2 = CreateFollower(color::Green, { 200, 200 });
		entity3 = CreateFollower(color::Blue, { 300, 300 });
		entity4 = CreateFollower(color::Yellow, { 400, 400 });
		entity5 = CreateFollower(color::Magenta, { 500, 500 });

		// Instant follow.
		config1.smooth_lerp_factor = { 1.0f, 1.0f };

		// Delayed lerp follow.
		config2.smooth_lerp_factor = { 0.5f, 0.5f };

		// Velocity follow.
		config3.move_mode = MoveMode::Velocity;

		// Path follow (lerp).
		config4.smooth_lerp_factor = { 0.5f, 0.5f };
		config4.loop_path		   = true;
		config4.stop_distance	   = 40.0f;

		// Path follow (velocity).
		config5.move_mode	  = MoveMode::Velocity;
		config5.loop_path	  = true;
		config5.stop_distance = 40.0f;

		auto e{ game.renderer.GetLogicalResolution() };
		auto c{ e / 2.0f };

		waypoints = { V2_float{}, c, V2_float{ e.x, 0 }, c, e, c, V2_float{ 0, e.y }, c };

		/*	StartFollow(entity1, mouse, config1);
			StartFollow(entity2, mouse, config2);
			StartFollow(entity3, mouse, config3);*/
		StartFollow(entity4, waypoints, config4);
		StartFollow(entity5, waypoints, config5);
	}

	void Update() override {
		SetPosition(mouse, input.GetMousePosition());
		if (input.MouseDown(Mouse::Left)) {
			/*StopFollow(entity1);
			StopFollow(entity2);
			StopFollow(entity3);*/
			StopFollow(entity4);
			StopFollow(entity5);
		} else if (input.MouseDown(Mouse::Right)) {
			/*	StartFollow(entity1, mouse, config1);
				StartFollow(entity2, mouse, config2);
				StartFollow(entity3, mouse, config3);*/
			StartFollow(entity4, waypoints, config4);
			StartFollow(entity5, waypoints, config5);
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("FollowEffectScene: Left/Right: Stop/Start Follow");
	game.scene.Enter<FollowEffectScene>("");
	return 0;
}