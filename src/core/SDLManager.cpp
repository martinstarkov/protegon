#include "SDLManager.h"

#include <cassert> // assert

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <SDL_mixer.h>

#include "debugging/Debug.h"

namespace ptgn {

namespace impl {

SDLManager::SDLManager() {
	auto sdl_flags{ 
		SDL_INIT_AUDIO |
		SDL_INIT_EVENTS |
		SDL_INIT_TIMER |
		SDL_INIT_VIDEO
	};
	if (!SDL_WasInit(sdl_flags)) {
		auto sdl_init{ SDL_Init(sdl_flags) };
		if (sdl_init != 0) {
			debug::PrintLine("SDL_Init: ", SDL_GetError());
			abort();
		}
		auto img_flags{ IMG_INIT_PNG | IMG_INIT_JPG	};
		auto img_init{ IMG_Init(img_flags) };
		if ((img_init & img_flags) != img_flags) {
			debug::PrintLine("IMG_Init: Failed to init required png and jpg support!");
			debug::PrintLine("IMG_Init: ", IMG_GetError());
			abort();
		}
		auto ttf_init{ TTF_Init() };
		if (ttf_init == -1) {
			debug::PrintLine("TTF_Init: ", TTF_GetError());
			abort();
		}
	}
}

SDLManager::~SDLManager() {
	TTF_Quit();
	IMG_Quit();
	SDL_Quit();
}

SDLManager& GetSDLManager() {
	static SDLManager sdl_manager;
	return sdl_manager;
}

} // namespace impl

} // namespace ptgn