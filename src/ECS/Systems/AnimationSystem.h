#pragma once

#include "System.h"

#include "SDL.h"

// Take StateComponent (somehow) and set current SpriteComponent sprite equal to the corresponding spritesheet image

class AnimationSystem : public System<AnimationComponent, SpriteComponent, StateMachineComponent> {
public:
	virtual void update() override {
		//LOG_("Animating[" << _entities.size() << "],");
		for (auto& entityID : _entities) {
			Entity& e = getEntity(entityID);
			AnimationComponent* animation = e.getComponent<AnimationComponent>();
			SpriteComponent* sprite = e.getComponent<SpriteComponent>();
			StateMachineComponent* sm = e.getComponent<StateMachineComponent>();
			BaseStateMachine* walkStateMachine = sm->_stateMachines[0];
			BaseStateMachine* jumpStateMachine = sm->_stateMachines[1];
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
				animation->_counter = 0;
			}
			if (jumpStateID == typeid(JumpState).hash_code()) {
				animationTile.y = 3;
			}
			if (jumpStateMachine->stateChangeOccured()) {
				animation->_counter = 0;
			}
			unsigned int totalTimer = animation->_counter % (animation->_framesBetween * animation->_sprites);
			unsigned int stageProgress = totalTimer % animation->_framesBetween;
			if (stageProgress == 0) {
				stageProgress = totalTimer / animation->_framesBetween;
				animationTile.x = (float)stageProgress;
				sprite->_source.x = sprite->_source.w * (int)animationTile.x;
			}
			animation->_counter++;

			sprite->_source.y = sprite->_source.h * (int)animationTile.y;
			//LOG_(" [" << animation->_counter << "," << totalTimer << "] ");
		}
	}
};