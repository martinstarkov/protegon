#pragma once

#include "System.h"

#include <SDL.h>

// TODO: Take StateComponent (somehow) and set current SpriteComponent sprite equal to the corresponding spritesheet image

class AnimationSystem : public ecs::System<AnimationComponent, SpriteComponent, SpriteSheetComponent, DirectionComponent> {
public:
	virtual void Update() override final {
		for (auto [entity, anim, sprite, spriteSheet, dir] : entities) {
			//LOG("Direction: " << (int)direction->direction);
			if (anim.name != "" && (anim.counter == -1 || dir.direction != dir.previousDirection)) {
				SpriteInformation spriteInfo = spriteSheet.getSpriteInformation(anim.name, dir.direction);
				sprite.source.y = static_cast<int>(sprite.source.h * spriteInfo.start.y);
				anim.sprites = spriteInfo.count;
				anim.counter = static_cast<int>(anim.cyclesPerFrame * spriteInfo.start.x);
				//LOG("animation: sprites = " << animation->sprites << "; counter = " << animation->counter << "; start = " << spriteInfo.start);
			}
			unsigned int totalTimer = anim.counter % (anim.cyclesPerFrame * anim.sprites);
			if (totalTimer % anim.cyclesPerFrame == 0) {
				anim.frame = static_cast<int>(totalTimer / anim.cyclesPerFrame);
				sprite.source.x = static_cast<int>(sprite.source.w * anim.frame);
			}
			if (totalTimer == 0) { // reset counter so it doesn't grow infinitely
				anim.counter = 0;
			}
			anim.counter++;
		}
	}
};