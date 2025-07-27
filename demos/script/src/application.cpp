#include "core/entity.h"
#include "core/game.h"
#include "core/script.h"
#include "core/time.h"
#include "debug/log.h"
#include "events/input_handler.h"
#include "events/key.h"
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
		entity.AddTimerScript<TimedScript>(seconds{ 3 });
		// entity.AddTimerScript<TimedScript>(seconds{ 0 });

		// Errors:
		// entity.AddTimerScript<TimedScript>(seconds{ -3 });

		// entity.AddRepeatScript<RepeatedScript>(seconds{ 2 }, -1, false);
		// entity.AddRepeatScript<RepeatedScript>(seconds{ 2 }, -1, true);
		// entity.AddRepeatScript<RepeatedScript>(seconds{ 2 }, 3, true);
		// entity.AddRepeatScript<RepeatedScript>(seconds{ 2 }, 3, false);
		// entity.AddRepeatScript<RepeatedScript>(seconds{ 2 }, 1, true);
		// entity.AddRepeatScript<RepeatedScript>(seconds{ 2 }, 1, false);
		// entity.AddRepeatScript<RepeatedScript>(seconds{ 0 }, 1, false);
		// entity.AddRepeatScript<RepeatedScript>(seconds{ 0 }, 3, true);

		// Errors:
		// entity.AddRepeatScript<RepeatedScript>(seconds{ 2 }, 0, false);
		// entity.AddRepeatScript<RepeatedScript>(seconds{ 2 }, -2, false);
		// entity.AddRepeatScript<RepeatedScript>(seconds{ -1 }, 1, false);
	}

	void Update() override {
		if (game.input.KeyDown(Key::E)) {
			entity.RemoveScript<RepeatedScript>();
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("ScriptScene");
	game.scene.Enter<ScriptScene>("");
	return 0;
}