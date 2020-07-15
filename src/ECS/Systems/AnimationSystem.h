#pragma once

#include "System.h"

#include "SDL.h"

// Take StateComponent (somehow) and set current SpriteComponent sprite equal to the corresponding spritesheet image

class AnimationSystem : public System<AnimationComponent, SpriteComponent, StateMachineComponent> {
public:
	virtual void update() override {
		for (auto& entityID : entities) {
			Entity& e = getEntity(entityID);
			AnimationComponent* animation = e.getComponent<AnimationComponent>();
			SpriteComponent* sprite = e.getComponent<SpriteComponent>();
			StateMachineComponent* sm = e.getComponent<StateMachineComponent>();
			BaseStateMachine* walkStateMachine = sm->stateMachines[0];
			BaseStateMachine* jumpStateMachine = sm->stateMachines[1];
			StateID walkStateID = walkStateMachine->getCurrentState()->getStateID();
			StateID jumpStateID = jumpStateMachine->getCurrentState()->getStateID();
			static Vec2D animationTile = Vec2D();
			if (walkStateID == typeid(IdleState).hash_code()) {
				animationTile.y = 0;
			} else if (walkStateID == typeid(WalkState).hash_code()) {
				animationTile.y = 1;
			} else if (walkStateID == typeid(RunState).hash_code()) {
				animationTile.y = 2;
			}
			if (walkStateMachine->stateChangeOccured()) {
				animation->counter = 0;
			}
			if (jumpStateID == typeid(JumpState).hash_code()) {
				animationTile.y = 3;
			}
			if (jumpStateMachine->stateChangeOccured()) {
				animation->counter = 0;
			}
			unsigned int totalTimer = animation->counter % (animation->cyclesPerFrame * animation->sprites);
			unsigned int stageProgress = totalTimer % animation->cyclesPerFrame;
			if (stageProgress == 0) {
				stageProgress = totalTimer / animation->cyclesPerFrame;
				animationTile.x = static_cast<double>(stageProgress);
				sprite->source.x = static_cast<int>(sprite->source.w * animationTile.x);
				animation->state = static_cast<int>(animationTile.x);
			}
			if (totalTimer == 0) { // reset counter so it doesn't grow infinitely
				animation->counter = 0;
			}
			animation->counter++;

			sprite->source.y = static_cast<int>(sprite->source.h * animationTile.y);
			//LOG_(" [" << animation->counter << "," << totalTimer << "] ");
		}
	}
};