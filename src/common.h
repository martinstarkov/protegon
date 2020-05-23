#pragma once
#include "SDL.h"

enum class Axis {
	HORIZONTAL = 0,
	VERTICAL = 1,
	BOTH = 2,
	NEITHER = -1
};

enum class Side {
	TOP = -1,
	BOTTOM = 1,
	LEFT = -1,
	RIGHT = 1,
	ANY = 0
};

enum class Direction {
	LEFT = SDL_FLIP_HORIZONTAL,
	RIGHT = SDL_FLIP_NONE,
	DOWN = SDL_FLIP_NONE,
	UP = SDL_FLIP_VERTICAL
};