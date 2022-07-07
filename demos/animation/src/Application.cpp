#include <iostream>

#include "core/Engine.h"
#include "animation/SpriteMap.h"
#include "animation/Offset.h"
#include "managers/RendererManager.h"
#include "managers/TextureManager.h"
#include "utils/Countdown.h"
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
		const auto& renderer_manager{ managers::GetManager<managers::RendererManager>() };
		bool draw = renderer_manager.Has(key_);
		auto renderer = renderer_manager.Get(key_);
		draw &= renderer != nullptr;
		if (draw) {
		}
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
		const auto& renderer_manager{ managers::GetManager<managers::RendererManager>() };
		bool draw = renderer_manager.Has(key_);
		auto renderer = renderer_manager.Get(key_);
		static animation::SpriteMap sprite_map{ *renderer, "map1", "resources/spritesheet.png" };
		draw &= renderer != nullptr;
		if (draw) {
			auto texture_key = sprite_map.GetTextureKey();
			const auto& texture_manager{ managers::GetManager<managers::TextureManager>() };
			assert(texture_manager.Has(texture_key));
			auto texture = texture_manager.Get(texture_key);
			assert(texture != nullptr);
			V2_int animation_position{ test_animation.top_left.x + test_animation.size.x * test_animation.frame, test_animation.top_left.y };
			renderer->DrawTexture(*texture, position, size, animation_position, test_animation.size);
			if (animation_countdown.Finished()) {
				++test_animation.frame;
				animation_countdown.Start();
			}
			if (test_animation.frame >= test_animation.frames) {
				test_animation.frame = 0;
			}
		}
	}
};

int main(int c, char** v) {
	AnimationTest test;
	test.Start("test", "Animation Test", { 400, 400 });
	
	return 0;
}