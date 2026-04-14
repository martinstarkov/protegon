
#include "runtime/scripting/script_sequence.h"

#include <chrono>

#include "app/application.h"
#include "core/log.h"
#include "core/input/key.h"
#include "runtime/animation/tween.h"
#include "runtime/ecs/entity.h"

#include "runtime/scene/scene.h"
#include "runtime/scene/scene_input.h"
#include "runtime/scripting/script.h"

using namespace ptgn;

struct ScriptSequence2 : public Script {
	void OnEvent(EventDispatcher d) override {
		d.Dispatch<event::TweenProgress>(&ScriptSequence2::OnProgress, this);
	}

	void OnProgress(float progress) const {
		PTGN_LOG("2: 200 ms: ", progress);
	}
};

class ScriptSequenceScene : public Scene {
public:
	ScriptSequence s1;

	void OnEnter() override {
		using namespace std::literals::chrono_literals;

		During(*this, 100ms, []() { PTGN_LOG("During 100ms!"); });
		After(*this, 4000ms, []() { PTGN_LOG("After 4000ms Completed!"); });

		s1 = CreateScriptSequence(*this);
		s1.Then([]() { PTGN_LOG("1: Start"); });
		s1.During(200ms, []() { PTGN_LOG("1: 200 ms"); });
		s1.Then([]() { PTGN_LOG("1: Before waiting..."); });
		s1.Wait(3000ms);
		s1.Then([]() { PTGN_LOG("1: Completed!"); });
		s1.Start();

		auto s2 = CreateScriptSequence(*this);
		s2.Then([]() { PTGN_LOG("2: Start"); });
		s2.During<ScriptSequence2>(200ms);
		s2.Then([]() { PTGN_LOG("2: Before waiting..."); });
		s2.Wait(3000ms);
		s2.Then([]() { PTGN_LOG("2: Completed!"); });
		s2.Start();
	}

	void OnUpdate() override {
		if (ctx().input.KeyPressed(Key::I)) {
			PTGN_LOG("Entity Count: ", GetEntityCount());
		}
	}
};

int main(int, char**) {
	Application game{ "ScriptSequenceScene" };
	game.StartWith<ScriptSequenceScene>();
}