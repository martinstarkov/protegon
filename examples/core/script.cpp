#include "runtime/scripting/script.h"

#include "app/application.h"
#include "core/graphics/color.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "platform/input/input_handler.h"
#include "platform/input/key.h"
#include "renderer/renderer.h"
#include "runtime/ecs/components/draw.h"
#include "runtime/ecs/components/transform_component.h"
#include "runtime/ecs/entity.h"
#include "runtime/input/movement.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_manager.h"

using namespace ptgn;

class PlayerController : public Script<PlayerController> {
public:
	V2_float vel;

	void OnUpdate() override {
		MoveWASD(vel, V2_float{ 10.0f } * app().DeltaTime(), true);
		Translate(entity, vel);
	}
};

class RemoveScript : public Script<RemoveScript, KeyScript> {
public:
	void OnKeyPressed(Key k) override {
		if (k == Key::Q) {
			TryAddScript<PlayerController>(entity);
		}
		if (k == Key::E) {
			RemoveScripts<PlayerController>(entity);
		}
	}
};

struct ScriptScene : public Scene {
	Entity entity;

	void OnEnter() override {
		entity = CreateRect(*this, {}, { 30, 30 }, color::Red);

		AddScript<RemoveScript>(entity);
		AddScript<PlayerController>(entity);
	}
};

int main(int, char**) {
	Application game{ "ScriptScene: WASD: move, Q/E: add/remove script" };
	game.StartWith<ScriptScene>();
}