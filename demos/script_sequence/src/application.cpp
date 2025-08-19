
#include "core/game.h"
#include "core/script_sequence.h"
#include "input/input_handler.h"
#include "input/key.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

class ScriptSequenceScene : public Scene {
public:
	ScriptSequence s1;

	void Enter() override {
		using namespace std::literals::chrono_literals;

		s1 = CreateScriptSequence(*this);
		s1.Then([](Entity e) { PTGN_LOG("Start 1"); });
		s1.During(500ms, [](Entity e) { PTGN_LOG("500 ms"); });
		s1.Then([](Entity e) { PTGN_LOG("Before waiting..."); });
		s1.Wait(3000ms);
		s1.Then([](Entity e) { PTGN_LOG("Completed!"); });
		s1.Start();
	}

	void Update() override {
		if (game.input.KeyPressed(Key::I)) {
			PTGN_LOG(s1.IsAlive());
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("ScriptSequenceScene");
	game.scene.Enter<ScriptSequenceScene>("");
	return 0;
}