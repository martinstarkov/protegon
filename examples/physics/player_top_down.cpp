#include "app/application.h"
#include "core/event/dispatcher.h"
#include "renderer/primitives/color.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/shape.h"
#include "runtime/physics/movement.h"
#include "runtime/physics/collider.h"
#include "runtime/physics/rigid_body.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_manager.h"
#include "runtime/scripting/script.h"
#include "runtime/scripting/scripts.h"

using namespace ptgn;

constexpr V2_int game_size{ 960, 540 };

constexpr ColliderMask ground_mask{ 1 };

struct TopDownScript1 : public Script {
	void OnEvent(EventDispatcher d) override {
		d.Dispatch<PlayerMoveStart>([this](const PlayerMoveStart& m) { OnMoveStart(m); });
		d.Dispatch<PlayerMoveStop>([this](const PlayerMoveStop& m) { OnMoveStop(m); });
		d.Dispatch<PlayerMoveHeld>([this](const PlayerMoveHeld& m) { OnMove(m); });
		d.Dispatch<PlayerMoveDirectionChange>([this](const PlayerMoveDirectionChange& m) {
			OnDirectionChange(m);
		});
	}

	void OnMoveStart(const PlayerMoveStart& m) {
		PTGN_LOG("OnMoveStart: ", m.direction);
	}

	void OnMove(const PlayerMoveHeld& m) {
		PTGN_LOG("OnMove: ", m.direction);
	}

	void OnMoveStop(const PlayerMoveStop& m) {
		PTGN_LOG("OnMoveStop: ", m.last_direction);
	}

	void OnDirectionChange(const PlayerMoveDirectionChange& m) {
		PTGN_LOG("OnDirectionChange: difference: ", m.difference);
	}
};

class TopDownMovementScene : public Scene {
	Entity CreateWall(const V2_float& position, const V2_float& size, Origin origin) {
		Entity entity = CreateRect(*this, position, size, color::Purple, -1.0f, origin);
		auto& box	  = entity.Add<Collider>(Rect{ size });
		SetDrawOrigin(entity, origin);
		box.SetMask(ground_mask);
		return entity;
	}

	Entity CreatePlayer() {
		Entity entity = CreateRect(
			*this, V2_float{ 100, 100 }, V2_float{ 20, 40 }, color::DarkGreen, -1.0f, Origin::Center
		);
		AddScript<TopDownScript1>(entity);
		auto& rb = entity.Add<RigidBody>();
		auto& m	 = entity.Add<TopDownMovement>();
		auto& b	 = entity.Add<Collider>(Rect{ V2_float{ 20, 40 } });
		b.SetCollisionMode(CollisionMode::Continuous);
		return entity;
	}

	void OnEnter() override {
		// TODO: Fix.
		// SetColliderVisibility(true);

		V2_float ws{ game_size };

		CreatePlayer();
		CreateWall(-ws * 0.5f + V2_float{ 0, ws.y - 10 }, { ws.x, 10 }, Origin::TopLeft);
		CreateWall(-ws * 0.5f + V2_float{ 0, ws.y / 2.0f }, { 200, 10 }, Origin::TopLeft);
		CreateWall(-ws * 0.5f + V2_float{ ws.x, ws.y / 2.0f }, { 200, 10 }, Origin::TopRight);
		CreateWall(
			-ws * 0.5f + V2_float{ ws.x - 200, ws.y / 2.0f + 140 }, { ws.x - 400, 10 },
			Origin::TopRight
		);
	}
};

int main(int, char**) {
	Application app{ "TopDownMovementScene: WASD to move", game_size };
	app.StartWith<TopDownMovementScene>();
}