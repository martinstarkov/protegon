#include "app/application.h"
#include "core/event/dispatcher.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "renderer/primitives/color.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/shape.h"
#include "runtime/physics/collider.h"
#include "runtime/physics/collision_handler.h"
#include "runtime/physics/movement.h"
#include "runtime/physics/physics.h"
#include "runtime/physics/rigid_body.h"
#include "runtime/scene/scene.h"

#include "runtime/scripting/script.h"
#include "runtime/scripting/scripts.h"

using namespace ptgn;

constexpr V2_int game_size{ 960, 540 };

constexpr ColliderMask ground_mask{ 1 };

class GroundScript : public Script {
public:
	void OnEvent(EventDispatcher d) override {
		d.Dispatch<CollisionEvent>([this](const auto& e) { Ground(e.collision); });
	}

	void Ground(const Collision& c) const {
		if (c.normal == V2_float{ 0.0f, -1.0f }) {
			PlatformerJump::Ground(entity, c, ground_mask);
		}
	}
};

class PlatformingScene : public Scene {
	Entity CreatePlatform(const V2_float& position, const V2_float& size, Origin origin) {
		auto entity = CreateRect(*this, position, size, color::Purple, -1.0f, origin);
		auto& box	= entity.Add<Collider>(Rect{ size });
		box.SetMask(ground_mask);
		return entity;
	}

	Entity CreatePlayer() {
		auto entity = CreateRect(
			*this, V2_float{ 100, 100 }, V2_float{ 20, 40 }, color::DarkGreen, -1.0f, Origin::Center
		);
		auto& rb   = entity.Add<RigidBody>();
		rb.gravity = 1.0f;
		entity.Add<PlatformerMovement>();
		entity.Add<PlatformerJump>();
		auto& b = entity.Add<Collider>(Rect{ V2_float{ 20, 40 } });
		b.SetCollisionMode(CollisionMode::Continuous);
		AddScript<GroundScript>(entity);
		return entity;
	}

	void OnEnter() override {
		ctx().collision.SetSettings({ .debug_draw_ccd = true, .debug_draw_enabled = true });

		V2_float ws{ game_size };
		ctx().physics.SetGravity({ 0.0f, 1.0f });

		CreatePlayer();
		CreatePlatform(-ws * 0.5f + V2_float{ 0, ws.y - 10 }, { ws.x, 10 }, Origin::TopLeft);
		CreatePlatform(-ws * 0.5f + V2_float{ 0, ws.y / 2.0f }, { 200, 10 }, Origin::TopLeft);
		CreatePlatform(-ws * 0.5f + V2_float{ ws.x, ws.y / 2.0f }, { 200, 10 }, Origin::TopRight);
		CreatePlatform(
			-ws * 0.5f + V2_float{ ws.x - 200, ws.y / 2.0f + 140 }, { ws.x - 400, 10 },
			Origin::TopRight
		);
	}
};

int main(int, char**) {
	Application app{ "PlatformingScene", game_size };
	app.StartWith<PlatformingScene>();
}