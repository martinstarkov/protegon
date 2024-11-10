#include <memory>
#include <vector>

#include "collision/collider.h"
#include "common.h"
#include "components/sprite.h"
#include "components/transform.h"
#include "core/game.h"
#include "ecs/ecs.h"
#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "physics/movement.h"
#include "physics/rigid_body.h"
#include "renderer/color.h"
#include "renderer/origin.h"

constexpr CollisionCategory ground_category{ 1 };

class PlatformingTest : public Test {
	ecs::Manager manager;

	ecs::Entity CreatePlatform(const Rect& r) {
		ecs::Entity entity = manager.CreateEntity();
		entity.Add<Transform>(r.position, r.rotation);
		auto& box = entity.Add<BoxCollider>(entity, r.size, r.origin);
		box.SetCollisionCategory(ground_category);
		entity.Add<DrawColor>(color::Purple);
		return entity;
	}

	ecs::Entity CreatePlayer() {
		ecs::Entity entity = manager.CreateEntity();

		entity.Add<Transform>(ws / 2.0f + V2_float{ 100, 100 });
		auto& rb		 = entity.Add<RigidBody>();
		rb.gravity		 = 1.0f;
		auto& m			 = entity.Add<PlatformerMovement>();
		auto& j			 = entity.Add<PlatformerJump>();
		auto ground_func = [](Collision c) {
			PlatformerJump::Ground(c, ground_category);
		};
		auto& b				 = entity.Add<BoxCollider>(entity, V2_float{ 55, 129 }, Origin::Center);
		b.on_collision_start = ground_func;
		b.on_collision		 = ground_func;
		b.continuous		 = true;

		entity.Add<DrawColor>(color::DarkGreen);
		entity.Add<DrawLineWidth>(-1.0f);

		return entity;
	}

	void Init() final {
		manager.Clear();
		game.window.SetSize({ 960, 540 });
		ws = game.window.GetSize();

		CreatePlayer();
		CreatePlatform({ { 0, ws.y - 10 }, { ws.x, 10 }, Origin::TopLeft });
		CreatePlatform({ { 0, ws.y / 2.0f }, { 200, 10 }, Origin::TopLeft });
		CreatePlatform({ { ws.x, ws.y / 2.0f }, { 200, 10 }, Origin::TopRight });
		manager.Refresh();
	}

	void Update() final {
		game.physics.Update(manager);
		Draw();
	}

	void Draw() {
		for (auto [e, b] : manager.EntitiesWith<BoxCollider>()) {
			DrawRect(e, b.GetAbsoluteRect());
		}
	}
};

void TestPlatforming() {
	std::vector<std::shared_ptr<Test>> tests;

	tests.emplace_back(new PlatformingTest());

	AddTests(tests);
}