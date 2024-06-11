#include "sdl_instance.h"

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <SDL_mixer.h>

#include "protegon/debug.h"
#include "game.h"

namespace ptgn {

SDLInstance::SDLInstance() {
	InitSDL();
	InitSDLImage();
	InitSDLTTF();
	InitSDLMixer();
	InitWindow();
	InitRenderer();
}

SDLInstance::~SDLInstance() {
	Mix_CloseAudio();
	Mix_Quit();
	TTF_Quit();
	IMG_Quit();
	SDL_Quit();
}

bool SDLInstance::WindowExists() const {
	return window_ != nullptr;
}

std::shared_ptr<SDL_Window> SDLInstance::GetWindow() const {
	PTGN_ASSERT(window_ != nullptr, "Cannot access uninitialized or destroyed SDL_Window");
	return window_;
}

std::shared_ptr<SDL_Renderer> SDLInstance::GetRenderer() const {
	PTGN_ASSERT(renderer_ != nullptr, "Cannot access uninitialized or destroyed SDL_Renderer");
	return renderer_;
}

Color SDLInstance::GetWindowBackgroundColor() const {
	return window_bg_color_;
}

void SDLInstance::SetWindowBackgroundColor(const Color& new_color) {
	window_bg_color_ = new_color;
}

void SDLInstance::SetScale(const V2_float& new_scale) {
	scale_ = new_scale;
}

V2_float SDLInstance::GetScale() const {
	return scale_;
}

void SDLInstance::SetResolution(const V2_int& new_resolution) {
	resolution_ = new_resolution;
}

V2_int SDLInstance::GetResolution() const {
	return resolution_;
}

void SDLInstance::InitSDL() {
	std::uint32_t sdl_flags{
		SDL_INIT_AUDIO   |
		SDL_INIT_EVENTS  |
		SDL_INIT_TIMER   |
		SDL_INIT_VIDEO   |
		SDL_VIDEO_OPENGL
	};
	if (!SDL_WasInit(sdl_flags)) {
		SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_SCALING, "1");
		// Ensures window and elements scale by monitor zoom level for constant appearance.
		SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
		SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");

		int sdl_init{ SDL_Init(sdl_flags) };
		if (sdl_init != 0) {
			PTGN_ERROR(SDL_GetError());
			PTGN_CHECK(false, "Failed to initialize SDL core");
		}
	}
}

void SDLInstance::InitSDLImage() {
	int img_flags{
		IMG_INIT_PNG |
		IMG_INIT_JPG
	};
	int img_init{ IMG_Init(img_flags) };
	if ((img_init & img_flags) != img_flags) {
		PTGN_ERROR(IMG_GetError());
		PTGN_CHECK(false, "Failed to initialize SDL Image");
	}
}

void SDLInstance::InitSDLTTF() {
	int ttf_init{ TTF_Init() };
	if (ttf_init == -1) {
		PTGN_ERROR(TTF_GetError());
		PTGN_CHECK(false, "Failed to initialize SDL TTF");
	}
}

void SDLInstance::InitSDLMixer() {
	int mixer_init{ Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) };
	if (mixer_init == -1) {
		PTGN_ERROR(Mix_GetError());
		PTGN_CHECK(false, "Failed to initialize SDL Mixer");
	}
}

void SDLInstance::InitWindow() {
	window_ = { SDL_CreateWindow("", 0, 0, 0, 0, SDL_WINDOW_HIDDEN), SDL_DestroyWindow };
	if (window_ == nullptr) {
		PTGN_ERROR(SDL_GetError());
		PTGN_ASSERT(false, "Failed to create SDL window");
	}
}

void SDLInstance::InitRenderer() {
	renderer_ = { SDL_CreateRenderer(window_.get(), -1, SDL_RENDERER_ACCELERATED), SDL_DestroyRenderer };
	if (renderer_ == nullptr) {
		PTGN_ERROR(SDL_GetError());
		PTGN_ASSERT(false, "Failed to create SDL renderer");
	}
}

} // namespace ptgn