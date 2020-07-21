#include "AnimationSystem.h"

#include "../Components/AnimationComponent.h"
#include "../Components/SpriteComponent.h"

#include "SDL.h"

void AnimationSystem::update() {
	for (auto& id : entities) {
		EntityHandle e = EntityHandle(id, manager);
		AnimationComponent* animation = e.getComponent<AnimationComponent>();
		SpriteComponent* sprite = e.getComponent<SpriteComponent>();
		unsigned int totalTimer = animation->counter % (animation->cyclesPerFrame * animation->sprites);
		unsigned int stageProgress = totalTimer % animation->cyclesPerFrame;
		if (stageProgress == 0) {
			stageProgress = totalTimer / animation->cyclesPerFrame;
			sprite->source.x = static_cast<int>(sprite->source.w * stageProgress);
			animation->state = static_cast<int>(stageProgress);
		}
		if (totalTimer == 0) { // reset counter so it doesn't grow infinitely
			animation->counter = 0;
		}
		animation->counter++;
	}
}
