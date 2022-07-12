#include <iostream>

#include "core/Engine.h"
#include "animation/SpriteMap.h"
#include "animation/AnimationMap.h"
#include "managers/TextureManager.h"
#include "animation/Offset.h"
#include "renderer/Renderer.h"
#include "utility/Countdown.h"
#include "input/Input.h"
#include "math/Hash.h"
#include "state/StateMachine.h"
#include "event/Observer.h"

using namespace ptgn;

class AnimationTest : public Engine {
public:
	V2_int size{ 64, 64 };
	std::vector<V2_int> positions = { { 200, 200 }, { 100, 200 } };
	animation::SpriteMap sprite_map{ "map1", "resources/spritesheet.png" };
	animation::AnimationMap animation_map;
	managers::TextureManager& texture_manager{ managers::GetManager<managers::TextureManager>() };
	state::StateMachine state_machine;
	struct MovementState : public event::Event<MovementState> {
		MovementState(animation::AnimationState* animation) : animation{ animation } {}
		animation::AnimationState* animation;
	};
	virtual void Init() {
		sprite_map.Load(math::Hash("idle_animation"), V2_int{ 0, 0 + 1 * 16 + 1 }, V2_int{ 16, 16 }, 3, milliseconds{ 300 });
		sprite_map.Load(math::Hash("jump_animation"), V2_int{ 0, 0 }, V2_int{ 16, 16 }, 8, milliseconds{ 200 });
		animation_map.Load(0, sprite_map, math::Hash("idle_animation"), 0, true);



		state_machine.AddState("idle", [&](MovementState& state) {
			if (state_machine.GetCurrentState() == "jump") {
				std::cout << "Entering idle" << std::endl;
			    state.animation->SetAnimation(math::Hash("idle_animation"), 0);
			}
		});
		state_machine.AddState("jump", [&](MovementState& state) {
			if (state_machine.GetCurrentState() == "idle") {
				std::cout << "Entering jump" << std::endl;
				state.animation->SetAnimation(math::Hash("jump_animation"), 0);
			}
		});
		state_machine.PushState("idle");

	}
	virtual void Update(double dt) {
		auto state = animation_map.Get(0);
		draw::Texture(*texture_manager.Get(state->sprite_map.GetTextureKey()), positions[0], size, state->GetCurrentPosition(), state->GetAnimation().frame_size);
		if (input::KeyDown(Key::W)) {
			state_machine.Notify("jump", MovementState{ state });
		} if (input::KeyDown(Key::S)) {
			state_machine.Notify("idle", MovementState{ state });
		}
		animation_map.Update();
	}
};

int main(int c, char** v) {
	AnimationTest test;
	test.Start("Animation Test", { 400, 400 });
	return 0;
}