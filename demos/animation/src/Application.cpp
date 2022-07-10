#include <iostream>

#include "core/Engine.h"
#include "animation/SpriteMap.h"
#include "managers/TextureManager.h"
#include "animation/Offset.h"
#include "renderer/Renderer.h"
#include "utility/Countdown.h"
#include "input/Input.h"
#include "math/Hash.h"

using namespace ptgn;

class AnimationTest : public Engine {
public:
	V2_int size = { 64, 64 };
	V2_int position = { 200, 200 };
	V2_int position2 = { 100, 200 };
	animation::SpriteMap sprite_map{ "map1", "resources/spritesheet.png" };
	animation::AnimationMap animation_map;
	std::size_t anim1 = math::Hash("anim1");
	std::size_t anim2 = math::Hash("anim2");
	std::size_t map1 = math::Hash("map1");
	managers::TextureManager& texture_manager{ managers::GetManager<managers::TextureManager>() };
	animation::AnimationState state;
	virtual void Init() {
		auto& animation = sprite_map.Load(anim1, V2_int{ 0, 0 + 1 * 16 + 1 }, V2_int{ 16, 16 }, 3, milliseconds{ 400 });
		animation_map.Load(anim1, &animation, 0, true);
		animation_map.Load(anim2, &animation, 2, true);
	}
	virtual void Update(double dt) {
		auto animation = sprite_map.Get(anim1);
		auto state = animation_map.Get(anim1);
		auto state2 = animation_map.Get(anim2);
		draw::Texture(*texture_manager.Get(sprite_map.GetTextureKey()), position, size, state->GetCurrentPosition(), state->animation->frame_size);
		draw::Texture(*texture_manager.Get(sprite_map.GetTextureKey()), position2, size, state2->GetCurrentPosition(), state2->animation->frame_size);
		state->Update();
		state2->Update();
	}
};

int main(int c, char** v) {
	AnimationTest test;
	test.Start("Animation Test", { 400, 400 });
	test.Stop();
	
	return 0;
}