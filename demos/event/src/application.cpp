#include <functional>

#include "common/assert.h"
#include "core/game.h"
#include "core/window.h"
#include "debug/log.h"
#include "input/input_handler.h"
#include "math/vector2.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int window_size{ 800, 800 };

class OtherScene : public Scene {
	void Enter() {
		PTGN_LOG("Entered other scene");
	}
};

struct TestScript : public Script<TestScript> {
	void OnKeyDown(Key k) {
		if (entity.GetId() == 4) {
			if (k == Key::R) {
				// PTGN_LOG("Removing test script from ", entity.GetId());
				// entity.RemoveScript<TestScript>();

				// entity.Destroy();
				// PTGN_LOG("Destroying entity: ", entity.GetId());
				game.scene.Transition<OtherScene>("", "other", {});
			} else {
				PTGN_WARN("Should not be here after pressing R");
			}
		} else {
			PTGN_LOG("Key down script for: ", entity.GetId(), ", key: ", k);
		}
	}
};

struct TestScript2 : public Script<TestScript2> {
	void OnKeyDown(Key k) {
		PTGN_LOG("Key down on ", entity.GetId(), ": ", k);
	}

	void OnKeyPressed(Key k) {
		PTGN_LOG("Key pressed on ", entity.GetId(), ": ", k);
	}

	void OnKeyUp(Key k) {
		PTGN_LOG("Key up on ", entity.GetId(), ": ", k);
	}
};

class EventScene : public Scene {
public:
	Entity e1;
	Entity e2;

	void Enter() override {
		game.window.SetSetting(WindowSetting::Resizable);

		e1 = CreateEntity();
		e2 = CreateEntity();
		e1.AddScript<TestScript>();
		e1.AddScript<TestScript2>();
		e2.AddScript<TestScript>();
		e2.AddScript<TestScript2>();
	}

	void Update() override {}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("EventScene", window_size);
	game.scene.Enter<EventScene>("");
	return 0;
}