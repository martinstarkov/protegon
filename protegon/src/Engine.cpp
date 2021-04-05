#include "Engine.h"

#include <iostream>

#include <SDL.h>

namespace engine {

void DoSomething() {
	SDL_Init(SDL_INIT_EVERYTHING);
	std::cout << "hi" << std::endl;
}

}