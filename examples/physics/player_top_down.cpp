#include "app/application.h"
#include "core/log.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "renderer/primitives/color.h"
#include "runtime/ecs/entity.h"
#include "runtime/event/event_dispatcher.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/shape.h"
#include "runtime/physics/collider.h"
#include "runtime/physics/collision_handler.h"
#include "runtime/physics/move_direction.h"
#include "runtime/physics/movement.h"
#include "runtime/physics/rigid_body.h"
#include "runtime/scene/scene.h"
#include "runtime/scripting/script.h"
#include "runtime/scripting/scripts.h"

using namespace ptgn;

constexpr V2_int game_size{ 960, 540 };

constexpr ColliderMask ground_mask{ 1 };

struct TopDownScript1 : public Script {
	void OnEvent(EventDispatcher d) override {
		d.Dispatch<event::PlayerMoveStart>(&OnMoveStart, this);
		d.Dispatch<event::PlayerMoveStop>(&OnMoveStop, this);
		d.Dispatch<event::PlayerMoveHeld>(&OnMove, this);
		d.Dispatch<event::PlayerMoveDirectionChange>(&OnDirectionChange, this);
	}

	void OnMoveStart(MoveDirection direction) {
		PTGN_LOG("OnMoveStart: ", direction);
	}

	void OnMove(MoveDirection direction) {
		PTGN_LOG("OnMove: ", direction);
	}

	void OnMoveStop(MoveDirection last_direction) {
		PTGN_LOG("OnMoveStop: ", last_direction);
	}

	void OnDirectionChange(const event::PlayerMoveDirectionChange& change) {
		PTGN_LOG(
			"OnDirectionChange difference: ", change.difference,
			", current dir: ", change.current_direction
		);
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
		ctx().collision.SetSettings({ .debug_draw_enabled = true });

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