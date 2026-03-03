#include "runtime/scripting/script.h"

#include "app/application.h"
#include "app/context.h"
#include "core/event/dispatcher.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "platform/input/events.h"
#include "platform/input/key.h"
#include "renderer/primitives/color.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/shape.h"
#include "runtime/physics/movement.h"
#include "runtime/scene/scene.h"
#include "runtime/scripting/scripts.h"

using namespace ptgn;

class PlayerController : public Script {
public:
	V2_float vel;

	// TODO: Fix update script not being called.
	void OnUpdate() {
		MoveWASD(
			entity.GetScene(), vel, V2_float{ 10.0f } * entity.GetScene().app().DeltaTime().count(),
			true
		);
		Translate(entity, vel);
	}
};

class RemoveScript : public Script {
public:
	void OnEvent(EventDispatcher d) override {
		d.Dispatch<KeyPressed>([this](auto e) { OnKeyPressed(e.key); });
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