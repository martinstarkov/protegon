
#include "core/app/application.h"
#include "core/scripting/script_sequence.h"
#include "core/input/input_handler.h"
#include "core/input/key.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

struct ScriptSequence2 : public Script<ScriptSequence2, TweenScript> {
	void OnProgress(float progress) {
		PTGN_LOG("2: 200 ms");
	}
};

class ScriptSequenceScene : public Scene {
public:
	ScriptSequence s1;

	void Enter() override {
		using namespace std::literals::chrono_literals;

		During(*this, 100ms, [](Entity) { PTGN_LOG("During 100ms!"); });
		After(*this, 4000ms, [](Entity) { PTGN_LOG("After 4000ms Completed!"); });

		s1 = CreateScriptSequence(*this);
		s1.Then([](Entity) { PTGN_LOG("1: Start"); });
		s1.During(200ms, [](Entity) { PTGN_LOG("1: 200 ms"); });
		s1.Then([](Entity) { PTGN_LOG("1: Before waiting..."); });
		s1.Wait(3000ms);
		s1.Then([](Entity) { PTGN_LOG("1: Completed!"); });
		s1.Start();

		auto s2 = CreateScriptSequence(*this);
		s2.Then([](Entity) { PTGN_LOG("2: Start"); });
		s2.During<ScriptSequence2>(200ms);
		s2.Then([](Entity) { PTGN_LOG("2: Before waiting..."); });
		s2.Wait(3000ms);
		s2.Then([](Entity) { PTGN_LOG("2: Completed!"); });
		s2.Start();
	}

	void Update() override {
		if (input.KeyPressed(Key::I)) {
			PTGN_LOG("Entity Count: ", Size());
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	Application::Get().Init("ScriptSequenceScene");
	Application::Get().scene_.Enter<ScriptSequenceScene>("");
	return 0;
}