#include "Engine.h"

#include <iostream>

#include <SDL.h>

namespace engine {

void DoStuff() {
	SDL_Init(SDL_INIT_EVERYTHING);
	std::cout << "Init complete" << std::endl;
}

}