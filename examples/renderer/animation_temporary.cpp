#include <chrono>

#include "app/application.h"
#include "core/graphics/color.h"
#include "core/input/key.h"
#include "core/log.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "runtime/animation/animation.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/graphics/shape.h"
#include "runtime/physics/collision_handler.h"
#include "runtime/physics/movement.h"
#include "runtime/physics/rigid_body.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_input.h"

using namespace ptgn;

class AnimationTemporaryScene : public Scene {
	Entity player;

	void OnEnter() override {
		ctx().asset.Load("anim", "assets/animation_bubble.png");
		ctx().collision.SetDebugSettings({ .draw_enabled = true });

		player = CreateRect(
			*this, V2_float{ 100, 100 }, V2_float{ 20, 40 }, color::Orange, Solid{}, Origin::Center
		);
		player.Add<RigidBody>();
		player.Add<TopDownMovement>();
	}

	void OnUpdate() override {
		if (ctx().input.KeyPressed(Key::Space) && !HasChild(player, "love_bubble")) {
			auto anim = PlayTemporaryAnimation(
				*this, "anim", { 0, -50 },
				{ .frame_count = 4, .animation_duration = 1s, .play_count = 2 }, 2s
			);
			SetScale(anim, 3);
			AddChild(player, anim, "love_bubble");
		}

		PTGN_LOG("Entity count: ", GetEntityCount());
	}
};

int main(int, char**) {
	Application app{ "AnimationTemporaryScene: WASD to move, Space to spawn animation" };
	app.StartWith<AnimationTemporaryScene>();
}