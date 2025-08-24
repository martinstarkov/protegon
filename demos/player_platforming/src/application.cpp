#include "components/draw.h"
#include "components/movement.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "math/geometry/rect.h"
#include "math/vector2.h"
#include "physics/collision/collider.h"
#include "physics/physics.h"
#include "physics/rigid_body.h"
#include "renderer/api/color.h"
#include "renderer/api/origin.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

// TODO: Fix this demo.

using namespace ptgn;

constexpr V2_int window_size{ 960, 540 };

constexpr CollisionCategory ground_category{ 1 };

class GroundScript : public Script<GroundScript, CollisionScript> {
public:
	GroundScript() {}

	void Ground(Collision c) {
		if (c.normal == V2_float{ 0.0f, -1.0f }) {
			PlatformerJump::Ground(entity, c, ground_category);
		}
	}

	void OnCollision(Collision c) override {
		Ground(c);
	}
};

class PlatformingScene : public Scene {
	Entity CreatePlatform(const V2_float& position, const V2_float& size, Origin origin) {
		auto entity = CreateRect(*this, position, size, color::Purple, -1.0f, origin);
		auto& box	= entity.Add<Collider>(Rect{ size });
		box.SetCollisionCategory(ground_category);
		return entity;
	}

	Entity CreatePlayer() {
		auto entity = CreateRect(
			*this, window_size / 2.0f + V2_float{ 100, 100 }, V2_float{ 20, 40 }, color::DarkGreen,
			-1.0f, Origin::Center
		);
		auto& rb   = entity.Add<RigidBody>();
		rb.gravity = 1.0f;
		auto& m	   = entity.Add<PlatformerMovement>();
		auto& j	   = entity.Add<PlatformerJump>();
		auto& b	   = entity.Add<Collider>(Rect{ V2_float{ 20, 40 } });
		b.SetCollisionMode(CollisionMode::Continuous);
		AddScript<GroundScript>(entity);
		return entity;
	}

	void Enter() override {
		SetColliderVisibility(true);
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