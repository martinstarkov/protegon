#pragma once

// window

#define WINDOW_TITLE "Protegon"
#define WINDOW_X SDL_WINDOWPOS_CENTERED
#define WINDOW_Y SDL_WINDOWPOS_CENTERED
#define WINDOW_W 800
#define WINDOW_H 600
#define WINDOW_FLAGS SDL_WINDOW_SHOWN //SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN

// framerate

constexpr unsigned int FPS = 60;
constexpr double SECOND_CHANGE_PER_FRAME = 1.0 / FPS;

// rendering

#define DEFAULT_RENDER_COLOR SDL_Color{ 255, 255, 255, 255 }

// motion
#define LOWEST_VELOCITY 0.1 // velocity below this value is set to 0
#define UNIVERSAL_DRAG 0.15