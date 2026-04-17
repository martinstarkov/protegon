#include "app/application.h"
#include "core/event/event.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/log.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/shape.h"
#include "runtime/physics/collider.h"
#include "runtime/physics/collision_handler.h"
#include "runtime/physics/move_direction.h"
#include "runtime/physics/movement.h"
#include "runtime/physics/movement_event.h"
#include "runtime/physics/rigid_body.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scripting/script.h"
#include "serialization/json/fwd.h"

using namespace ptgn;

constexpr V2_int game_size{ 960, 540 };

constexpr ColliderMask ground_mask{ 1 };

struct TopDownScript1 : public Script {
	void OnEvent(Event d) override {
		d.Dispatch<event::PlayerMoveStart>(&TopDownScript1::OnMoveStart, this);
		d.Dispatch<event::PlayerMoveStop>(&TopDownScript1::OnMoveStop, this);
		d.Dispatch<event::PlayerMoveHeld>(&TopDownScript1::OnMove, this);
		d.Dispatch<event::PlayerMoveDirectionChange>(&TopDownScript1::OnDirectionChange, this);
	}

	void OnMoveStart(MoveDirection direction) {
		PTGN_LOG("OnMoveStart: ", json(direction));
	}

	void OnMove(MoveDirection direction) {
		PTGN_LOG("OnMove: ", json(direction));
	}

	void OnMoveStop(MoveDirection last_direction) {
		PTGN_LOG("OnMoveStop: ", json(last_direction));
	}

	void OnDirectionChange(const event::PlayerMoveDirectionChange& change) {
		PTGN_LOG(
			"OnDirectionChange difference: ", json(change.difference),
			", current dir: ", json(change.current_direction)
		);
	}
};

class TopDownMovementScene : public Scene {
	Entity CreateWall(const V2_float& position, const V2_float& size, Origin origin) {
		Entity entity = CreateRect(*this, position, size, color::Purple, Solid{}, origin);
		auto& box	  = entity.Add<Collider>(Rect{ size });
		SetDrawOrigin(entity, origin);
		box.SetMask(ground_mask);
		return entity;
	}

	Entity CreatePlayer() {
		Entity entity = CreateRect(
			*this, V2_float{ 100, 100 }, V2_float{ 20, 40 }, color::DarkGreen, Solid{},
			Origin::Center
		);
		AddScript<TopDownScript1>(entity);
		auto& rb = entity.Add<RigidBody>();
		auto& m	 = entity.Add<TopDownMovement>();
		auto& b	 = entity.Add<Collider>(Rect{ V2_float{ 20, 40 } });
		b.SetCollisionMode(CollisionMode::Continuous);
		return entity;
	}

	void OnEnter() override {
		ctx().collision.SetDebugSettings({ .draw_enabled = true });

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