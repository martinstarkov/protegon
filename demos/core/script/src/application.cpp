#include "core/ecs/components/draw.h"
#include "core/ecs/components/movement.h"
#include "core/ecs/components/transform.h"
#include "core/ecs/entity.h"
#include "core/app/application.h"
#include "core/scripting/script.h"
#include "core/scripting/script_interfaces.h"
#include "core/util/time.h"
#include "debug/core/log.h"
#include "core/input/input_handler.h"
#include "core/input/key.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "renderer/renderer.h"
#include "world/scene/scene.h"
#include "world/scene/scene_manager.h"

using namespace ptgn;

class PlayerController : public Script<PlayerController> {
public:
	V2_float vel;

	void OnUpdate() override {
		MoveWASD(vel, V2_float{ 10.0f } * game.dt(), true);
		Translate(entity, vel);
	}
};

class RemoveScript : public Script<RemoveScript, KeyScript> {
public:
	void OnKeyDown(Key k) override {
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

	void Enter() override {
		entity = CreateRect(*this, {}, { 30, 30 }, color::Red);

		AddScript<RemoveScript>(entity);
		AddScript<PlayerController>(entity);
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("ScriptScene: WASD: move, Q/E: add/remove script");
	game.scene.Enter<ScriptScene>("");
	return 0;
}