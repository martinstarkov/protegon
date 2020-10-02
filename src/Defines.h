#pragma once

#include <limits>

// general
// TODO: Figure out how to serialize numeric_limits
constexpr double INFINITE = DBL_MAX; //std::numeric_limits<double>::infinity();

// framerate

constexpr std::size_t FPS = 60;
constexpr double SECOND_CHANGE_PER_FRAME = 1.0 / FPS;

// colors

#include <SDL.h>

// rendering

#define RENDER_COLOR WHITE

// motion
#define LOWEST_VELOCITY 0.1 // velocity below this value is set to 0