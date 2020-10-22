#pragma once

#include <cstdlib>

// Framerate.

constexpr std::size_t FPS = 60;
constexpr double SECOND_CHANGE_PER_FRAME = 1.0 / FPS;

// Motion.
#define LOWEST_VELOCITY 0.1 // Velocity below this value is set to 0.