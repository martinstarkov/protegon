#include <iostream>

#include "core/Engine.h"
#include "animation/SpriteMap.h"
#include "animation/AnimationMap.h"
#include "interface/Draw.h"
#include "animation/Offset.h"
#include "renderer/Renderer.h"
#include "utility/Countdown.h"
#include "interface/Input.h"
#include "math/Hash.h"
#include "state/StateMachine.h"
#include "event/Observer.h"
#include "utility/Log.h"

using namespace ptgn;

class JumpState {};
class IdleState {};

class StateTest : public Engine {
public:
	V2_int size{ 64, 64 };
	std::vector<V2_int> positions{ { 200, 200 }, { 100, 200 } };
	animation::SpriteMap sprite_map{ "map1", "resources/spritesheet.png" };
	animation::AnimationMap animation_map;
	state::StateMachine state_machine;
	virtual void Init() {
		sprite_map.Load(math::Hash("idle_animation"), V2_int{ 0, 0 + 1 * 16 + 1 }, V2_int{ 16, 16 }, 3, milliseconds{ 300 });
		sprite_map.Load(math::Hash("jump_animation"), V2_int{ 0, 0 }, V2_int{ 16, 16 }, 8, milliseconds{ 200 });
		animation_map.Load(0, sprite_map, math::Hash("idle_animation"), 0, true);
		state_machine.AddState<IdleState>([&](animation::AnimationState& animation) {
			if (!state_machine.IsState<IdleState>()) {
				state_machine.PopState();
				PrintLine("Idle");
				animation.SetAnimation(math::Hash("idle_animation"), 0);
			}
		});
		state_machine.AddState<JumpState>([&](animation::AnimationState& animation, int i) {
			if (!state_machine.IsState<JumpState>()) {
				state_machine.PopState();
				PrintLine("Jump: ", i);
				animation.SetAnimation(math::Hash("jump_animation"), 0);
			}
		});
		state_machine.PushState<IdleState>(*animation_map.Get(0));

	}
	virtual void Update(float dt) {
		auto state = animation_map.Get(0);
		draw::Texture(state->sprite_map.GetTextureKey(),
					  { positions[0], size },
					  { state->GetCurrentPosition(), state->GetAnimation()->frame_size });
		state_machine.Update([&]() {
			if (input::KeyPressed(Key::W)) {
				state_machine.PushState<JumpState>(*state, 4);
			} else if (input::KeyPressed(Key::S)) {
				state_machine.PushState<IdleState>(*state);
			}
		});
		animation_map.Update();
		//PrintLine("state: ", state_machine.GetState(), ", active state count: ", state_machine.GetActiveStateCount());
	}
};

int main(int c, char** v) {
	StateTest test;
	test.Start("Animation Test", { 400, 400 }, true, V2_int{}, window::Flags::NONE, true, false);
	return 0;
}