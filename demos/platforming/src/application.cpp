#include "components/movement.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "math/vector2.h"
#include "physics/collision/collider.h"
#include "physics/physics.h"
#include "physics/rigid_body.h"
#include "rendering/api/color.h"
#include "rendering/api/origin.h"
#include "rendering/graphics/rect.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int window_size{ 960, 540 };

constexpr CollisionCategory ground_category{ 1 };

class GroundScript : public Script<GroundScript> {
public:
	GroundScript() {}

	void Ground(Collision c) {
		if (c.normal == V2_float{ 0.0f, -1.0f }) {
			PlatformerJump::Ground(entity, c, ground_category);
		}
	}

	void OnCollisionStart(Collision c) override {
		Ground(c);
	}

	void OnCollision(Collision c) override {
		Ground(c);
	}
};

class PlatformingScene : public Scene {
	Entity CreatePlatform(const V2_float& position, const V2_float& size, Origin origin) {
		auto entity = CreateRect(*this, position, size, color::Purple, -1.0f, origin);
		entity.Enable();
		auto& box = entity.Add<BoxCollider>(size, origin);
		box.SetCollisionCategory(ground_category);
		return entity;
	}

	Entity CreatePlayer() {
		auto entity = CreateRect(
			*this, window_size / 2.0f + V2_float{ 100, 100 }, V2_float{ 20, 40 }, color::DarkGreen,
			-1.0f, Origin::Center
		);
		entity.Enable();
		auto& rb	 = entity.Add<RigidBody>();
		rb.gravity	 = 1.0f;
		auto& m		 = entity.Add<PlatformerMovement>();
		auto& j		 = entity.Add<PlatformerJump>();
		auto& b		 = entity.Add<BoxCollider>(V2_float{ 20, 40 }, Origin::Center);
		b.continuous = true;
		entity.AddScript<GroundScript>();
		return entity;
	}

	void Enter() override {
		V2_float ws{ window_size };
		physics.SetGravity({ 0.0f, 1.0f });

		CreatePlayer();
		CreatePlatform({ 0, ws.y - 10 }, { ws.x, 10 }, Origin::TopLeft);
		CreatePlatform({ 0, ws.y / 2.0f }, { 200, 10 }, Origin::TopLeft);
		CreatePlatform({ ws.x, ws.y / 2.0f }, { 200, 10 }, Origin::TopRight);
		CreatePlatform({ ws.x - 200, ws.y / 2.0f + 140 }, { ws.x - 400, 10 }, Origin::TopRight);
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("PlatformingScene", window_size);
	game.scene.Enter<PlatformingScene>("");
	return 0;
}