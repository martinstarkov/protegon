#include "app/application.h"
#include "app/editor.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/shape.h"
#include "runtime/physics/collider.h"
#include "runtime/physics/movement.h"
#include "runtime/physics/physics.h"
#include "runtime/physics/platformer_jump_registry.h"
#include "runtime/physics/rigid_body.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "tools/debug/debug_system.h"

using namespace ptgn;

constexpr V2_int logical_size{ 960, 540 };
constexpr ColliderMask ground_mask{ 1 };

class PlatformingScene : public Scene {
	Entity CreatePlatform(const V2_float& position, const V2_float& size, Origin origin) {
		auto entity = CreateRect(*this, position, size, color::Purple, Solid{}, origin);
		auto& box = entity.Add<Collider>(Rect{ size });
		box.SetMask(ground_mask);
		return entity;
	}

	Entity CreatePlayer() {
		auto entity =
			CreateRect(*this, { 100, 100 }, { 20, 40 }, color::DarkGreen, Solid{}, Origin::Center);

		entity.Add<RigidBody>();

		auto& collider{ entity.Add<Collider>(Rect{ 20, 40 }) };
		collider.SetCollisionMode(CollisionMode::Continuous);

		auto& movement{ entity.Add<PlatformerMovement>() };
		movement.grounding.direction = GroundingDirection::AgainstGravity;
		movement.grounding.masks = { ground_mask };

		SetPlatformerJumpController(entity, "standard");
		return entity;
	}

	void OnEnter() override {
		ctx().debug.settings.collision = { .draw_ccd = true, .draw_enabled = true };

		V2_float ws{ logical_size };
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
	Application app{ "PlatformingScene", logical_size };
	PTGN_WITH_EDITOR(app, true);
	app.StartWith<PlatformingScene>();
}
