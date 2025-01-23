#include "protegon/protegon.h"

using namespace ptgn;

constexpr V2_int window_size{ 960, 540 };

constexpr CollisionCategory ground_category{ 1 };

class TopDownMovementExample : public Scene {
	ecs::Entity CreateWall(const Rect& r) {
		ecs::Entity entity = manager.CreateEntity();
		entity.Add<Transform>(r.position, r.rotation);
		auto& box = entity.Add<BoxCollider>(entity, r.size, r.origin);
		box.SetCollisionCategory(ground_category);
		entity.Add<DrawColor>(color::Purple);
		return entity;
	}

	ecs::Entity CreatePlayer() {
		ecs::Entity entity = manager.CreateEntity();

		entity.Add<Transform>(window_size / 2.0f + V2_float{ 100, 100 });
		auto& rb	 = entity.Add<RigidBody>();
		auto& m		 = entity.Add<TopDownMovement>();
		auto& b		 = entity.Add<BoxCollider>(entity, V2_float{ 20, 40 }, Origin::Center);
		b.continuous = true;

		entity.Add<DrawColor>(color::DarkGreen);
		entity.Add<DrawLineWidth>(-1.0f);

		return entity;
	}

	void Enter() override {
		manager.Clear();

		V2_float ws{ window_size };

		CreatePlayer();
		CreateWall({ { 0, ws.y - 10 }, { ws.x, 10 }, Origin::TopLeft });
		CreateWall({ { 0, ws.y / 2.0f }, { 200, 10 }, Origin::TopLeft });
		CreateWall({ { ws.x, ws.y / 2.0f }, { 200, 10 }, Origin::TopRight });
		CreateWall({ { ws.x - 200, ws.y / 2.0f + 140 }, { ws.x - 400, 10 }, Origin::TopRight });
		manager.Refresh();
	}

	void Exit() override {
		manager.Clear();
	}

	void Update() override {
		for (auto [e, b] : manager.EntitiesWith<BoxCollider>()) {
			DrawRect(e, b.GetAbsoluteRect());
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("TopDownMovementExample: WASD to move", window_size);
	game.scene.Enter<TopDownMovementExample>("top_down_movement");
	return 0;
}