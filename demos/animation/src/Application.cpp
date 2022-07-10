#include <iostream>

#include "core/Engine.h"
#include "animation/SpriteMap.h"
#include "animation/Offset.h"
#include "managers/TextureManager.h"
#include "renderer/Renderer.h"
#include "utility/Countdown.h"
#include "event/Input.h"

using namespace ptgn;

class AnimationTest : public Engine {
public:
	animation::Animation test_animation{ { 0, 0 + 1 * 16 + 1 }, { 16, 16 }, 3 };
	V2_int hitbox_size;
	animation::Offset offset;
	V2_int position = { 200, 200 };
	V2_int velocity = {};
	V2_int size = { 64, 64 };
	virtual void Init() {
		hitbox_size = { 8, 8 };
		offset = { test_animation, hitbox_size, animation::Alignment::MIDDLE, animation::Alignment::MIDDLE };
	}
	virtual void Update(double dt) {

		double speed = 1000;
		if (input::KeyPressed(Key::A)) velocity.x = -speed;
		if (input::KeyPressed(Key::D)) velocity.x = speed;
		if (input::KeyPressed(Key::W)) velocity.y = -speed;
		if (input::KeyPressed(Key::S)) velocity.y = speed;
		if (!input::KeyPressed(Key::S) &&
			!input::KeyPressed(Key::W) &&
			!input::KeyPressed(Key::A) &&
			!input::KeyPressed(Key::D)) {
			velocity = {};
		}

		position += velocity * dt;

		static Countdown animation_countdown(milliseconds{ 400 }, true);
		static animation::SpriteMap sprite_map{ "map1", "resources/spritesheet.png" };
		auto texture_key = sprite_map.GetTextureKey();
		const auto& texture_manager{ managers::GetManager<managers::TextureManager>() };
		assert(texture_manager.Has(texture_key));
		auto texture = texture_manager.Get(texture_key);
		assert(texture != nullptr);
		V2_int animation_position{ test_animation.top_left_pixel.x + test_animation.frame_size.x * test_animation.current_frame, test_animation.top_left_pixel.y };
		Renderer::DrawTexture(*texture, position, size, animation_position, test_animation.frame_size);
		if (animation_countdown.Finished()) {
			++test_animation.current_frame;
			animation_countdown.Start();
		}
		if (test_animation.current_frame >= test_animation.frame_count) {
			test_animation.current_frame = 0;
		}
	}
};

int main(int c, char** v) {
	AnimationTest test;
	test.Start("Animation Test", { 400, 400 });
	test.Stop();
	
	return 0;
}