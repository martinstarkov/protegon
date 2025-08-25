#include "components/draw.h"
#include "components/movement.h"
#include "core/game.h"
#include "math/geometry/rect.h"
#include "math/vector2.h"
#include "physics/collision/collider.h"
#include "physics/rigid_body.h"
#include "renderer/api/color.h"
#include "renderer/api/origin.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int window_size{ 960, 540 };

constexpr CollisionCategory ground_category{ 1 };

struct TopDownScript1 : public Script<TopDownScript1, PlayerMoveScript> {
	virtual void OnMoveStart() {
		PTGN_LOG("OnMoveStart");
	}

	virtual void OnMove() {
		PTGN_LOG("OnMove");
	}

	virtual void OnMoveStop() {
		PTGN_LOG("OnMoveStop");
	}

	virtual void OnDirectionChange([[maybe_unused]] MoveDirection direction_difference) {
		PTGN_LOG("OnDirectionChange: ", direction_difference);
	}

	virtual void OnMoveUpStart() {
		PTGN_LOG("OnMoveUpStart");
	}

	virtual void OnMoveUp() {
		PTGN_LOG("OnMoveUp");
	}

	virtual void OnMoveUpStop() {
		PTGN_LOG("OnMoveUpStop");
	}

	virtual void OnMoveDownStart() {
		PTGN_LOG("OnMoveDownStart");
	}

	virtual void OnMoveDown() {
		PTGN_LOG("OnMoveDown");
	}

	virtual void OnMoveDownStop() {
		PTGN_LOG("OnMoveDownStop");
	}

	virtual void OnMoveLeftStart() {
		PTGN_LOG("OnMoveLeftStart");
	}

	virtual void OnMoveLeft() {
		PTGN_LOG("OnMoveLeft");
	}

	virtual void OnMoveLeftStop() {
		PTGN_LOG("OnMoveLeftStop");
	}

	virtual void OnMoveRightStart() {
		PTGN_LOG("OnMoveRightStart");
	}

	virtual void OnMoveRight() {
		PTGN_LOG("OnMoveRight");
	}

	virtual void OnMoveRightStop() {
		PTGN_LOG("OnMoveRightStop");
	}
};

class TopDownMovementScene : public Scene {
	Entity CreateWall(const V2_float& position, const V2_float& size, Origin origin) {
		Entity entity = CreateRect(*this, position, size, color::Purple, -1.0f, origin);
		auto& box	  = entity.Add<Collider>(Rect{ size });
		SetDrawOrigin(entity, origin);
		box.SetCollisionCategory(ground_category);
		return entity;
	}

	Entity CreatePlayer() {
		Entity entity = CreateRect(
			*this, window_size / 2.0f + V2_float{ 100, 100 }, V2_float{ 20, 40 }, color::DarkGreen,
			-1.0f, Origin::Center
		);
		AddScript<TopDownScript1>(entity);
		auto& rb = entity.Add<RigidBody>();
		auto& m	 = entity.Add<TopDownMovement>();
		auto& b	 = entity.Add<Collider>(Rect{ V2_float{ 20, 40 } });
		b.SetCollisionMode(CollisionMode::Continuous);
		return entity;
	}

	void Enter() override {
		SetColliderVisibility(true);

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