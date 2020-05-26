#pragma once
#include "Components.h"
#include "SDL.h"

class DirectionComponent : public Component {
public:
	DirectionComponent(SDL_RendererFlip direction = SDL_RendererFlip::SDL_FLIP_NONE) {
		_direction = direction;
	}
	void update() override {
		if (entity->has<MotionComponent>()) {
			if (entity->get<MotionComponent>()->getVelocity().x < 0) {
				_direction = SDL_RendererFlip::SDL_FLIP_HORIZONTAL;
			} else if (entity->get<MotionComponent>()->getVelocity().x > 0) {
				_direction = SDL_RendererFlip::SDL_FLIP_NONE;
			}
		}
	}
	SDL_RendererFlip getDirection() { return _direction; }
	void setDirection(SDL_RendererFlip direction) {
		_direction = direction;
	}
private:
	SDL_RendererFlip _direction;
};