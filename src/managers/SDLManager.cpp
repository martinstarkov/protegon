#include "SDLManager.h"

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <SDL_mixer.h>

#include "debugging/Debug.h"

namespace ptgn {

namespace managers {

template <typename T>
SDLManager<T>::SDLSystemManager::SDLSystemManager() {
	auto sdl_flags{
		SDL_INIT_AUDIO |
		SDL_INIT_EVENTS |
		SDL_INIT_TIMER |
		SDL_INIT_VIDEO
	};
	if (!SDL_WasInit(sdl_flags)) {
		auto sdl_init{ SDL_Init(sdl_flags) };
		if (sdl_init != 0) {
			debug::PrintLine(SDL_GetError());
			assert(!"Failed to initialize SDL Core");
		}
		auto img_flags{ IMG_INIT_PNG | IMG_INIT_JPG };
		auto img_init{ IMG_Init(img_flags) };
		if ((img_init & img_flags) != img_flags) {
			debug::PrintLine(IMG_GetError());
			assert(!"Failed to initialize SDL Image");
		}
		auto ttf_init{ TTF_Init() };
		if (ttf_init == -1) {
			debug::PrintLine(TTF_GetError());
			assert(!"Failed to initialize SDL TTF");
		}
		if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) == -1) {
			debug::PrintLine(Mix_GetError());
			assert(!"Failed to initialize SDL Mixer");
		}
	}
}

template <typename T>
SDLManager<T>::SDLSystemManager::~SDLSystemManager() {
	Mix_CloseAudio();
	Mix_Quit();
	TTF_Quit();
	IMG_Quit();
	SDL_Quit();
}

} // namespace managers

} // namespace ptgn