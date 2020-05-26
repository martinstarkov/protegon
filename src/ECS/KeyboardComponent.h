#pragma once
#include "Components.h"
#include "SDL.h"
#include "../InputHandler.h"
#include <array>

#define KEYS 4

class KeyboardComponent : public Component {
public:
	KeyboardComponent(Vec2D speed = Vec2D(), const std::array<SDL_Scancode, KEYS> keys = std::array<SDL_Scancode, KEYS>{ SDL_SCANCODE_W, SDL_SCANCODE_A, SDL_SCANCODE_S, SDL_SCANCODE_D }) : _keys(keys), _motionComponent(nullptr) {
		_speed = speed;
		_states = InputHandler::getKeyStates();
	}
	void init() override {
		_motionComponent = entity->get<MotionComponent>();
	}
	void update() override {
		_states = InputHandler::getKeyStates();
		motion();
		if (entity->has<ShootComponent>()) {
			if (_states[SDL_SCANCODE_C]) {
				entity->get<ShootComponent>()->setShooting(true);
			} else if (!_states[SDL_SCANCODE_C]) {
				entity->get<ShootComponent>()->setShooting(false);
			}
		}
	}
private:
	void motion() {
		if (_states[_keys[1]] && !_states[_keys[3]]) { // left
			_motionComponent->setAcceleration(Vec2D(-1.0f * _speed.x, _motionComponent->getAcceleration().y));
		}
		if (_states[_keys[3]] && !_states[_keys[1]]) { // right
			_motionComponent->setAcceleration(Vec2D(1.0f * _speed.x, _motionComponent->getAcceleration().y));
		}
		if ((_states[_keys[0]] || _states[SDL_SCANCODE_SPACE]) && !_states[_keys[2]]) { // up
			_motionComponent->setAcceleration(Vec2D(_motionComponent->getAcceleration().x, -1.0f * _speed.y));
		}
		if (_states[_keys[2]] && !(_states[_keys[0]] || _states[SDL_SCANCODE_SPACE])) { // down
			_motionComponent->setAcceleration(Vec2D(_motionComponent->getAcceleration().x, 1.0f * _speed.y));
		}
		if ((!_states[_keys[1]] && !_states[_keys[3]]) || (_states[_keys[1]] && _states[_keys[3]])) {
			_motionComponent->setAcceleration(Vec2D(0.0f, _motionComponent->getAcceleration().y));
		}
		if (!_states[_keys[0]] && !_states[_keys[2]] || (_states[_keys[0]] && _states[_keys[2]])) {
			_motionComponent->setAcceleration(Vec2D(_motionComponent->getAcceleration().x, 0.0f));
		}
	}
private:
	Vec2D _speed;
	const std::array<SDL_Scancode, KEYS> _keys;
	MotionComponent* _motionComponent;
	const Uint8* _states;
};