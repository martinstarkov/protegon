#pragma once

#include "System.h"

#include "SDL.h"

// TODO: Take StateComponent (somehow) and set current SpriteComponent sprite equal to the corresponding spritesheet image

class AnimationSystem : public System<AnimationComponent, SpriteComponent, SpriteSheetComponent, DirectionComponent> {
public:
	virtual void update() override final {
		for (auto& id : entities) {
			Entity e = Entity(id, manager);
			AnimationComponent* animation = e.getComponent<AnimationComponent>();
			SpriteComponent* sprite = e.getComponent<SpriteComponent>();
			SpriteSheetComponent* ss = e.getComponent<SpriteSheetComponent>();
			DirectionComponent* direction = e.getComponent<DirectionComponent>();
			//LOG("Direction: " << (int)direction->direction);
			if (animation->name != "" && (animation->counter == -1 || direction->direction != direction->previousDirection)) {
				SpriteInformation spriteInfo = ss->getSpriteInformation(animation->name, direction->direction);
				sprite->source.y = static_cast<int>(sprite->source.h * spriteInfo.start.y);
				animation->sprites = spriteInfo.count;
				animation->counter = static_cast<int>(animation->cyclesPerFrame * spriteInfo.start.x);
				//LOG("animation: sprites = " << animation->sprites << "; counter = " << animation->counter << "; start = " << spriteInfo.start);
			}
			unsigned int totalTimer = animation->counter % (animation->cyclesPerFrame * animation->sprites);
			if (totalTimer % animation->cyclesPerFrame == 0) {
				animation->frame = static_cast<int>(totalTimer / animation->cyclesPerFrame);
				sprite->source.x = static_cast<int>(sprite->source.w * animation->frame);
			}
			if (totalTimer == 0) { // reset counter so it doesn't grow infinitely
				animation->counter = 0;
			}
			animation->counter++;
		}
	}
};