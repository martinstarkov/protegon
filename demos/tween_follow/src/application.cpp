#include "components/sprite.h"
#include "components/transform.h"
#include "core/game.h"
#include "input/input_handler.h"
#include "math/vector2.h"
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

	TargetFollowConfig config1;
	TargetFollowConfig config2;
	TargetFollowConfig config3;
	PathFollowConfig config4;

	std::vector<V2_float> waypoints;

	void Enter() override {
		LoadResources({ { "smile1", "resources/smile1.png" },
						{ "smile2", "resources/smile2.png" },
						{ "smile3", "resources/smile3.png" },
						{ "smile4", "resources/smile4.png" } });

		mouse = CreateEntity();
		SetPosition(mouse, {});

		entity1 = CreateSprite(*this, "smile1", { 150, 150 }); // Red
		entity2 = CreateSprite(*this, "smile2", { 600, 600 }); // Green
		entity3 = CreateSprite(*this, "smile3", { 150, 600 }); // Blue
		entity4 = CreateSprite(*this, "smile4", { 150, 600 }); // Yellow

		// Red: Instant follow.
		config1.lerp_factor = { 1.0f, 1.0f };
		// Green: Delayed lerp follow.
		config2.lerp_factor = { 0.5f, 0.5f };
		// Blue: Velocity follow.
		config3.move_mode = MoveMode::Velocity;
		// Yellow: Path follow.
		config4.lerp_factor	  = { 0.5f, 0.5f };
		waypoints			  = { V2_float{ 0, 0 }, V2_float{ 400, 400 } };
		config4.stop_distance = 10.0f;

		StartFollow(entity1, mouse, config1);
		StartFollow(entity2, mouse, config2);
		StartFollow(entity3, mouse, config3);
		StartFollow(entity4, waypoints, config4);
	}

	void Update() override {
		SetPosition(mouse, input.GetMousePosition());
		if (input.MouseDown(Mouse::Left)) {
			StopFollow(entity1);
			StopFollow(entity2);
			StopFollow(entity3);
			StopFollow(entity4);
		} else if (input.MouseDown(Mouse::Right)) {
			StartFollow(entity1, mouse, config1);
			StartFollow(entity2, mouse, config2);
			StartFollow(entity3, mouse, config3);
			StartFollow(entity4, waypoints, config4);
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("FollowEffectScene: Left/Right: Stop/Start Follow");
	game.scene.Enter<FollowEffectScene>("");
	return 0;
}