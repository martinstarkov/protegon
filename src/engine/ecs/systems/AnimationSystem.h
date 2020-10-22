#pragma once

#include "System.h"

#include <SDL.h>

// TODO: Take StateComponent (somehow) and set current SpriteComponent sprite equal to the corresponding spritesheet image

class AnimationSystem : public ecs::System<AnimationComponent, SpriteComponent, SpriteSheetComponent, DirectionComponent> {
public:
	virtual void Update() override final {
		for (auto [entity, anim, sprite, sprite_sheet, dir] : entities) {
			//LOG("Direction: " << (int)direction->direction);
			if (anim.name != "" && (anim.counter == -1 || dir.direction != dir.previous_direction)) {
				SpriteInformation sprite_info = sprite_sheet.GetSpriteInformation(anim.name, dir.direction);
				sprite.source.position.y = static_cast<int>(sprite.source.size.y * sprite_info.start.y);
				anim.sprites = sprite_info.count;
				anim.counter = static_cast<int>(anim.cycles_per_frame * sprite_info.start.x);
				//LOG("animation: sprites = " << animation->sprites << "; counter = " << animation->counter << "; start = " << spriteInfo.start);
			}
			unsigned int total_timer = anim.counter % (anim.cycles_per_frame * anim.sprites);
			if (total_timer % anim.cycles_per_frame == 0) {
				anim.frame = static_cast<int>(total_timer / anim.cycles_per_frame);
				sprite.source.position.x = static_cast<int>(sprite.source.size.x * anim.frame);
			}
			if (total_timer == 0) { // reset counter so it doesn't grow infinitely
				anim.counter = 0;
			}
			anim.counter++;
		}
	}
};