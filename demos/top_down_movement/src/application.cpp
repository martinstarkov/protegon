#include "components/movement.h"
#include "core/game.h"
#include "math/vector2.h"
#include "physics/collision/collider.h"
#include "physics/rigid_body.h"
#include "rendering/api/color.h"
#include "rendering/api/origin.h"
#include "rendering/graphics/rect.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int window_size{ 960, 540 };

constexpr CollisionCategory ground_category{ 1 };

class TopDownMovementScene : public Scene {
	Entity CreateWall(const V2_float& position, const V2_float& size, Origin origin) {
		Entity entity = CreateRect(*this, position, size, color::Purple, -1.0f, origin);
		auto& box	  = entity.Add<BoxCollider>(size, origin);
		entity.Enable();
		box.SetCollisionCategory(ground_category);
		return entity;
	}

	Entity CreatePlayer() {
		Entity entity = CreateRect(
			*this, window_size / 2.0f + V2_float{ 100, 100 }, V2_float{ 20, 40 }, color::DarkGreen,
			-1.0f, Origin::Center
		);
		auto& rb	 = entity.Add<RigidBody>();
		auto& m		 = entity.Add<TopDownMovement>();
		auto& b		 = entity.Add<BoxCollider>(V2_float{ 20, 40 }, Origin::Center);
		b.continuous = true;
		entity.Enable();
		return entity;
	}

	void Enter() override {
		V2_float ws{ window_size };

		CreatePlayer();
		CreateWall({ 0, ws.y - 10 }, { ws.x, 10 }, Origin::TopLeft);
		CreateWall({ 0, ws.y / 2.0f }, { 200, 10 }, Origin::TopLeft);
		CreateWall({ ws.x, ws.y / 2.0f }, { 200, 10 }, Origin::TopRight);
		CreateWall({ ws.x - 200, ws.y / 2.0f + 140 }, { ws.x - 400, 10 }, Origin::TopRight);
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("TopDownMovementScene: WASD to move", window_size);
	game.scene.Enter<TopDownMovementScene>("");
	return 0;
}