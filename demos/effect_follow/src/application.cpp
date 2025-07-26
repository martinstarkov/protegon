#include "components/draw.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/manager.h"
#include "events/input_handler.h"
#include "math/vector2.h"
#include "rendering/api/color.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "tweening/tween_effects.h"

using namespace ptgn;

struct FollowEffectScene : public Scene {
	Sprite mouse;

	Sprite entity1;
	Sprite entity2;
	Sprite entity3;

	FollowConfig config1;
	FollowConfig config2;
	FollowConfig config3;

	void Enter() override {
		LoadResource("smile1", "resources/smile1.png");
		LoadResource("smile2", "resources/smile2.png");
		LoadResource("smile3", "resources/smile3.png");

		mouse = CreateEntity();
		mouse.SetPosition({});

		entity1 = CreateSprite(*this, "smile1"); // Red
		entity2 = CreateSprite(*this, "smile2"); // Green
		entity3 = CreateSprite(*this, "smile3"); // Blue

		entity1.SetPosition({ 150, 150 });
		entity2.SetPosition({ 600, 600 });
		entity3.SetPosition({ 150, 600 });

		config1.lerp_factor = { 1.0f, 1.0f };	  // Red
		config2.lerp_factor = { 0.5f, 0.5f };	  // Green.
		config3.move_mode	= MoveMode::Velocity; // Blue

		StartFollow(entity1, mouse, config1);
		StartFollow(entity2, mouse, config2);
		StartFollow(entity3, mouse, config3);
	}

	void Update() override {
		mouse.SetPosition(input.GetMousePosition());
		if (game.input.MouseDown(Mouse::Left)) {
			StopFollow(entity1);
			StopFollow(entity2);
			StopFollow(entity3);
		} else if (game.input.MouseDown(Mouse::Right)) {
			StartFollow(entity1, mouse, config1);
			StartFollow(entity2, mouse, config2);
			StartFollow(entity3, mouse, config3);
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("FollowEffectScene: left/right click to stop/start follow");
	game.scene.Enter<FollowEffectScene>("");
	return 0;
}