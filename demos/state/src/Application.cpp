#include <iostream>
#include "state/State.h"
#include "state/StateMachine.h"
#include "input/Input.h"
#include "core/Engine.h"
#include "core/ECS.h"

using namespace ptgn;

struct JumpState : public state::State {
	void Enter(ecs::Entity& player, int i) {
		std::cout << "Jump!" << std::endl;
	}
	void Exit() {
		std::cout << "Stopped Jump!" << std::endl;
	}
};
struct LandState : public state::State {
	void Enter() {
		std::cout << "Landed!" << std::endl;
	}
	void Exit() {
		std::cout << "No longer landed!" << std::endl;
	}
};

class MyEngine : public Engine {
public:
	state::StateMachine state_machine;
	virtual void Init() {
		state_machine.AddState<JumpState>();
		state_machine.AddState<LandState>();
		state_machine.SetState<LandState>();
	}
	ecs::Entity player;
	virtual void Update(double dt) {
		if (input::KeyDown(Key::W)) {
			int i;
			state_machine.SetState<JumpState>(player, i);
		}
		if (input::KeyDown(Key::S)) {
			state_machine.SetState<LandState>();
		}
	}
};

int main(int c, char** v) {
	MyEngine test;
	test.Start("State test", { 300, 300 });
	return 0;
}