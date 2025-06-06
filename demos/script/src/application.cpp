#include "core/entity.h"
#include "core/game.h"
#include "core/script.h"
#include "debug/log.h"
#include "math/vector2.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

class PlayerController : public Script<PlayerController> {
public:
	void OnUpdate(float dt) override {
		auto posx{ entity.GetPosition().x };
		if (posx < 100.0f) {
			MoveForward(dt);
			PTGN_LOG("Moving entity ", entity.GetUUID(), " to the right: ", posx);
		}
	}

	void MoveForward(float dt) {
		entity.SetPosition({ entity.GetPosition().x + dt * 5.0f, 0.0f });
	}
};

class FootstepSound : public Script<FootstepSound> {
public:
	void OnUpdate(float dt) override {
		PlaySound();
	}

	void PlaySound() const {
		PTGN_LOG("Playing sound for entity ", entity.GetUUID());
	}
};

struct ScriptScene : public Scene {
	Entity entity;

	void Enter() override {
		entity = CreateEntity();

		entity.AddScript<PlayerController>();
		entity.AddScript<FootstepSound>();
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("ScriptScene");
	game.scene.Enter<ScriptScene>("");
	return 0;
}