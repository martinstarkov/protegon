#include "protegon/protegon.h"

using namespace ptgn;

constexpr V2_int resolution{ 960, 540 };

constexpr CollisionCategory ground_category{ 1 };

class PlatformingExample : public Scene {
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

		entity.Add<Transform>(resolution / 2.0f + V2_float{ 100, 100 });
		auto& rb		 = entity.Add<RigidBody>();
		rb.gravity		 = 1.0f;
		auto& m			 = entity.Add<PlatformerMovement>();
		auto& j			 = entity.Add<PlatformerJump>();
		auto ground_func = [](Collision c) {
			PlatformerJump::Ground(c, ground_category);
		};
		auto& b				 = entity.Add<BoxCollider>(entity, V2_float{ 20, 40 }, Origin::Center);
		b.on_collision_start = ground_func;
		b.on_collision		 = ground_func;
		b.continuous		 = true;

		entity.Add<DrawColor>(color::DarkGreen);
		entity.Add<DrawLineWidth>(-1.0f);

		return entity;
	}

	void Init() override {
		manager.Clear();

		V2_float ws{ resolution };

		CreatePlayer();
		CreatePlatform({ { 0, ws.y - 10 }, { ws.x, 10 }, Origin::TopLeft });
		CreatePlatform({ { 0, ws.y / 2.0f }, { 200, 10 }, Origin::TopLeft });
		CreatePlatform({ { ws.x, ws.y / 2.0f }, { 200, 10 }, Origin::TopRight });
		CreatePlatform({ { ws.x - 200, ws.y / 2.0f + 140 }, { ws.x - 400, 10 }, Origin::TopRight });
		manager.Refresh();
	}

	void Shutdown() override {
		manager.Clear();
	}

	void Update() override {
		game.physics.Update(manager);
		
		for (auto [e, b] : manager.EntitiesWith<BoxCollider>()) {
			DrawRect(e, b.GetAbsoluteRect());
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("PlatformingExample", resolution);
	game.scene.LoadActive<PlatformingExample>("platforming");
	return 0;
}