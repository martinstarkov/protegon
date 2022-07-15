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
#include "utility/Log.h"
#include "scene/Camera.h"

using namespace ptgn;

class AnimationTest : public Engine {
public:
	V2_int size{ 16, 9 };
	std::vector<V2_double> positions = { { 200, 200 }, { 100, 200 } };
	animation::SpriteMap sprite_map{ "map1", "resources/spritesheet.png" };
	animation::AnimationMap animation_map;
	std::size_t anim1 = math::Hash("anim1");
	std::size_t anim2 = math::Hash("anim2");
	std::size_t map1 = math::Hash("map1");
	animation::Offset offset;
	V2_double velocity;
	Camera camera;
	managers::TextureManager& texture_manager{ managers::GetManager<managers::TextureManager>() };
	virtual void Init() {
		auto& animation = sprite_map.Load(anim1, V2_int{ 0, 0 + 1 * 16 }, V2_int{ 16, 16 }, 3, milliseconds{ 400 });
		animation_map.Load(0, sprite_map, anim1, 0, true);
		animation_map.Load(1, sprite_map, anim1, 2, true);
		offset = { animation.frame_size, size, animation::Alignment::CENTER, animation::Alignment::BOTTOM };
	}
	virtual void Update(double dt) {
		double speed = 100;
		//PrintLine(dt);

		if (input::KeyPressed(Key::A)) velocity.x = -speed;
		if (input::KeyPressed(Key::D)) velocity.x = speed;
		if (input::KeyPressed(Key::W)) velocity.y = -speed;
		if (input::KeyPressed(Key::S)) velocity.y = speed;
		if (input::KeyPressed(Key::Q)) camera.ZoomOut();
		if (input::KeyPressed(Key::E)) camera.ZoomIn();
		if ((input::KeyPressed(Key::A) && input::KeyPressed(Key::D)) || (!input::KeyPressed(Key::A) && !input::KeyPressed(Key::D))) velocity.x = 0;
		if ((input::KeyPressed(Key::W) && input::KeyPressed(Key::S)) || (!input::KeyPressed(Key::W) && !input::KeyPressed(Key::S))) velocity.y = 0;

		positions[0] += velocity * dt;

		animation_map.Update();

		camera.CenterOn(positions[0], size);

		for (auto i = 0; i < animation_map.Size(); ++i) {
			auto state = animation_map.Get(i);
			draw::Texture(*texture_manager.Get(state->sprite_map.GetTextureKey()), 
						  camera.RelativePosition(positions[i] - offset.value), 
						  camera.RelativeSize(state->GetAnimation().frame_size),
						  state->GetCurrentPosition(), 
						  state->GetAnimation().frame_size);
			draw::Rectangle(camera.RelativePosition(positions[i]), camera.RelativeSize(size), color::RED);
		}
	}
};

int main(int c, char** v) {
	AnimationTest test;
	test.Start("Animation Test", { 400, 400 });
	return 0;
}