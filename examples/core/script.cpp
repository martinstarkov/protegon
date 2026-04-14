#include "runtime/scripting/script.h"

#include "app/application.h"
#include "core/event/key_event.h"
#include "core/graphics/color.h"
#include "core/input/key.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/shape.h"
#include "runtime/physics/movement.h"
#include "runtime/scene/scene.h"

using namespace ptgn;

class PlayerController : public Script {
public:
	V2_float vel;

	static constexpr V2_float speed{ 10.0f };

	void OnUpdate() override {
		float dt{ entity.GetScene().ctx().dt().count() };
		MoveWASD(entity.GetScene(), vel, speed * dt, true);
		Translate(entity, vel);
	}
};

class RemoveScript : public Script {
public:
	void OnEvent(Event d) override {
		d.Dispatch<event::KeyPressed>(&RemoveScript::OnKeyPressed, this);
	}

	void OnKeyPressed(Key k) {
		if (k == Key::Q) {
			AddScript<PlayerController>(entity);
		}
		if (k == Key::E) {
			ptgn::RemoveScript<PlayerController>(entity);
		}
	}
};

struct ScriptScene : public Scene {
	Entity entity;

	void OnEnter() override {
		entity = CreateRect(*this, {}, { 30, 30 }, color::Red);

		AddScript<::RemoveScript>(entity);
		AddScript<PlayerController>(entity);
	}
};

int main(int, char**) {
	Application game{ "ScriptScene: WASD: move, Q/E: add/remove script" };
	game.StartWith<ScriptScene>();
}