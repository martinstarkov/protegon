#include "sdl_instance.h"

#include <cassert> // assert

#include "protegon/log.h"

namespace ptgn {

namespace global {

namespace hidden {

std::unique_ptr<SDLInstance> sdl{ nullptr };

} // namespace hidden

void InitSDL() {
	hidden::sdl = std::make_unique<SDLInstance>();
}

void DestroySDL() {
	hidden::sdl.reset();
}

SDLInstance& GetSDL() {
	return *hidden::sdl;
}

} // namespace global

SDLInstance::SDLInstance() {
	InitSDL();
	InitSDLImage();
	InitSDLTTF();
	InitSDLMixer();
	InitWindow();
	InitRenderer();
}

void SDLInstance::InitSDL() {
	SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
	auto sdl_flags{	SDL_INIT_AUDIO | SDL_INIT_EVENTS |
		            SDL_INIT_TIMER | SDL_INIT_VIDEO };
	if (!SDL_WasInit(sdl_flags)) {
		auto sdl_init{ SDL_Init(sdl_flags) };
		if (sdl_init != 0) {
			PrintLine(SDL_GetError());
			assert(!"Failed to initialize SDL core");
		}
	}
}

void SDLInstance::InitSDLImage() {
	auto img_flags{ IMG_INIT_PNG | IMG_INIT_JPG };
	auto img_init{ IMG_Init(img_flags) };
	if ((img_init & img_flags) != img_flags) {
		PrintLine(IMG_GetError());
		assert(!"Failed to initialize SDL Image");
	}
}

void SDLInstance::InitSDLTTF() {
	auto ttf_init{ TTF_Init() };
	if (ttf_init == -1) {
		PrintLine(TTF_GetError());
		assert(!"Failed to initialize SDL TTF");
	}
}

void SDLInstance::InitSDLMixer() {
	auto mixer_init{ Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) };
	if (mixer_init == -1) {
		PrintLine(Mix_GetError());
		assert(!"Failed to initialize SDL Mixer");
	}
}

void SDLInstance::InitWindow() {
	window_ = SDL_CreateWindow("", 0, 0, 0, 0, SDL_WINDOW_HIDDEN);
	if (window_ == nullptr) {
		PrintLine(SDL_GetError());
		assert(!"Failed to create SDL window");
	}
}

void SDLInstance::InitRenderer() {
	renderer_ = SDL_CreateRenderer(window_, -1, 0);
	SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
	if (renderer_ == nullptr) {
		PrintLine(SDL_GetError());
		assert(!"Failed to create SDL renderer");
	}
}

SDLInstance::~SDLInstance() {
	SDL_DestroyRenderer(renderer_);
	SDL_DestroyWindow(window_);
	Mix_CloseAudio();
	Mix_Quit();
	TTF_Quit();
	IMG_Quit();
	SDL_Quit();
}

SDL_Window* SDLInstance::GetWindow() const {
	return window_;
}

SDL_Renderer* SDLInstance::GetRenderer() const {
	return renderer_;
}

} // namespace ptgn