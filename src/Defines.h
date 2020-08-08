#pragma once

#include <limits>

// general
// TODO: Figure out how to serialize numeric_limits
constexpr double INFINITE = DBL_MAX; //std::numeric_limits<double>::infinity();

// window

#define WINDOW_TITLE "Protegon"
#define WINDOW_W 608
#define WINDOW_H 416
#define WINDOW_X 1280 - WINDOW_W
#define WINDOW_Y 30
#define WINDOW_FLAGS SDL_WINDOW_SHOWN //SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN

// framerate

constexpr std::size_t FPS = 60;
constexpr double SECOND_CHANGE_PER_FRAME = 1.0 / FPS;

// colors

#include "SDL.h"

#define TRANSPARENT SDL_Color{ 0, 0, 0, 0 }
#define WHITE SDL_Color{ 255, 255, 255, 255 }
#define BLACK SDL_Color{ 0, 0, 0, 255 }
#define RED SDL_Color{ 255, 0, 0, 255 }
#define DARK_RED SDL_Color{ 128, 0, 0, 255 }
#define ORANGE SDL_Color{ 255, 165, 0, 255 }
#define YELLOW SDL_Color{ 255, 255, 0, 255 }
#define GOLD SDL_Color{ 255, 215, 0, 255 }
#define GREEN SDL_Color{ 0, 128, 0, 255 }
#define LIME SDL_Color{ 0, 255, 0, 255 }
#define DARK_GREEN SDL_Color{ 0, 100, 0, 255 }
#define BLUE SDL_Color{ 0, 0, 255, 255 }
#define DARK_BLUE SDL_Color{ 0, 0, 128, 255 }
#define CYAN SDL_Color{ 0, 255, 255, 255 }
#define TEAL SDL_Color{ 0, 128, 128, 255 }
#define MAGENTA SDL_Color{ 255, 0, 255, 255 }
#define PURPLE SDL_Color{ 128, 0, 128, 255 }
#define PINK SDL_Color{ 255, 192, 203, 255 }
#define GREY SDL_Color{ 128, 128, 128, 255 }
#define SILVER SDL_Color{ 192, 192, 192, 255 }

// rendering

#define RENDER_COLOR WHITE

// motion
#define LOWEST_VELOCITY 0.1 // velocity below this value is set to 0