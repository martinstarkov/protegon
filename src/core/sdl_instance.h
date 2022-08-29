#include "protegon/engine.h"

#include <SDL.h>

namespace ptgn {

class SDLInstance {
	SDLInstance() = default;

private:
	SDL_Window* window{ nullptr };
	SDL_Renderer* renderer{ nullptr };
};

} // namespace ptgn