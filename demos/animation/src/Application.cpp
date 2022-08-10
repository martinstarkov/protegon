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


using namespace ptgn;

class AnimationTest : public Engine {
public:
	V2_int size{ 64, 64 };
	std::vector<V2_int> positions{ { 200, 200 }, { 100, 200 } };
	animation::SpriteMap sprite_map{ "map1", "resources/spritesheet.png" };
	animation::AnimationMap animation_map;
	std::size_t anim1{ math::Hash("anim1") };
	std::size_t anim2{ math::Hash("anim2") };
	std::size_t map1{ math::Hash("map1") };
	virtual void Init() {
		sprite_map.Load(anim1, V2_int{ 0, 0 + 1 * 16 + 1 }, V2_int{ 16, 16 }, 3, milliseconds{ 400 });
		animation_map.Load(0, sprite_map, anim1, 0, true);
		animation_map.Load(1, sprite_map, anim1, 2, true);
	}
	virtual void Update(float dt) {
		for (auto i{ 0 }; i < animation_map.Size(); ++i) {
			auto state = animation_map.Get(i);
			draw::Texture(state->sprite_map.GetTextureKey(), { positions[i], size }, { state->GetCurrentPosition(), state->GetAnimation()->frame_size });
		}
		animation_map.Update();
	}
};

int main(int c, char** v) {
	AnimationTest test;
	test.Start("Animation Test", { 400, 400 }, true, V2_int{}, window::Flags::NONE, true, false);
	return 0;
}