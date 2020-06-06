#pragma once

#include "System.h"

#include "SDL.h"

class AnimationSystem : public System<AnimationComponent, SpriteComponent> {
public:
	virtual void update() override {
		//std::cout << "Animating[" << _entities.size() << "],";
		for (auto& entityID : _entities) {
			Entity& e = getEntity(entityID);
			AnimationComponent* animation = e.getComponent<AnimationComponent>();
			SpriteComponent* sprite = e.getComponent<SpriteComponent>();
			if (sprite->_sprites > 1) {
				unsigned int totalTimer = animation->_counter % (animation->_framesBetween * sprite->_sprites);
				unsigned int stageProgress = totalTimer % animation->_framesBetween;
				if (stageProgress == 0) {
					stageProgress = totalTimer / animation->_framesBetween;
					sprite->_source.x = sprite->_source.w * stageProgress;
				}
				animation->_counter++;
			}
		}
	}
};