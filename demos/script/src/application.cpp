#include "core/entity.h"
#include "core/game.h"
#include "core/script.h"
#include "core/script_registry.h"
#include "core/time.h"
#include "debug/log.h"
#include "input/input_handler.h"
#include "input/key.h"
#include "math/vector2.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

class PlayerController : public Script<PlayerController> {
public:
	void OnUpdate() override {
		auto posx{ GetPosition(entity).x };
		if (posx < 100.0f) {
			MoveForward(game.dt());
			PTGN_LOG("Moving entity ", entity.GetUUID(), " to the right: ", posx);
		}
	}

	void MoveForward(float dt) {
		SetPosition(entity, { GetPosition(entity).x + dt * 5.0f, 0.0f });
	}
};

class TimedScript : public Script<TimedScript> {
public:
	void OnTimerStart() override {
		PTGN_LOG("Timed script started");
	}

	void OnTimerUpdate(float elapsed_fraction) override {
		PTGN_LOG("Timed script: ", elapsed_fraction);
	}

	bool OnTimerStop() override {
		PTGN_LOG("Timed script stopped");
		return true;
	}
};

class RepeatedScript : public Script<RepeatedScript> {
public:
	void OnRepeatStart() override {
		PTGN_LOG("Repeated script started");
	}

	void OnRepeatUpdate(int repeat) override {
		PTGN_LOG("Repeated script: ", repeat);
	}

	void OnRepeatStop() override {
		PTGN_LOG("Repeated script stopped");
	}
};

struct ScriptScene : public Scene {
	Entity entity;

	void Enter() override {
		entity = CreateEntity();

		// entity.AddScript<PlayerController>();

		// TODO: Make on stop get 1.0 completion and on start get 0.0.
		AddTimerScript<TimedScript>(entity, seconds{ 3 });
		// AddTimerScript<TimedScript>(entity, seconds{ 0 });

		// Errors:
		// AddTimerScript<TimedScript>(entity, seconds{ -3 });

		// AddRepeatScript<RepeatedScript>(entity, seconds{ 2 }, -1, false);
		// AddRepeatScript<RepeatedScript>(entity, seconds{ 2 }, -1, true);
		// AddRepeatScript<RepeatedScript>(entity, seconds{ 2 }, 3, true);
		// AddRepeatScript<RepeatedScript>(entity, seconds{ 2 }, 3, false);
		// AddRepeatScript<RepeatedScript>(entity, seconds{ 2 }, 1, true);
		// AddRepeatScript<RepeatedScript>(entity, seconds{ 2 }, 1, false);
		// AddRepeatScript<RepeatedScript>(entity, seconds{ 0 }, 1, false);
		// AddRepeatScript<RepeatedScript>(entity, seconds{ 0 }, 3, true);

		// Errors:
		// AddRepeatScript<RepeatedScript>(entity, seconds{ 2 }, 0, false);
		// AddRepeatScript<RepeatedScript>(entity, seconds{ 2 }, -2, false);
		// AddRepeatScript<RepeatedScript>(entity, seconds{ -1 }, 1, false);
	}

	void Update() override {
		if (game.input.KeyDown(Key::E)) {
			RemoveScript<RepeatedScript>(entity);
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("ScriptScene");
	game.scene.Enter<ScriptScene>("");
	return 0;
}