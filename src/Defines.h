#pragma once

#include <limits>

// framerate

constexpr std::size_t FPS = 60;
constexpr double SECOND_CHANGE_PER_FRAME = 1.0 / FPS;

// motion
#define LOWEST_VELOCITY 0.1 // velocity below this value is set to 0