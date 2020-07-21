#pragma once

#include "Component.h"

#include "SDL.h"

struct DirectionComponent : public Component<DirectionComponent> {
	SDL_RendererFlip xDirection;
	SDL_RendererFlip yDirection;
	DirectionComponent(SDL_RendererFlip xDirection = SDL_FLIP_NONE, SDL_RendererFlip yDirection = SDL_FLIP_NONE) : xDirection(xDirection), yDirection(yDirection) {}
};