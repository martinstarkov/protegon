#include "core/sdl_instance.h"

#include <chrono>
#include <cstdint>
#include <iosfwd>
#include <ostream>
#include <thread>

#include "common/assert.h"
#include "core/time.h"
#include "debug/log.h"
#include "rendering/gl/gl_renderer.h"
#include "SDL.h"
#include "SDL_error.h"
#include "SDL_hints.h"
#include "SDL_image.h"
#include "SDL_mixer.h"
#include "SDL_timer.h"
#include "SDL_ttf.h"
#include "SDL_version.h"
#include "SDL_video.h"

inline std::ostream& operator<<(std::ostream& os, const SDL_version& v) {
	os << static_cast<int>(v.major) << "." << static_cast<int>(v.minor) << "."
	   << static_cast<int>(v.patch);
	return os;
}

namespace ptgn::impl {

bool SDLInstance::IsInitialized() const {
	return SDLIsInitialized() && SDLImageIsInitialized() && SDLTTFIsInitialized() &&
		   SDLMixerIsInitialized();
}

bool SDLInstance::SDLMixerIsInitialized() const {
	return sdl_mixer_init_;
}

bool SDLInstance::SDLTTFIsInitialized() const {
	return sdl_ttf_init_;
}

bool SDLInstance::SDLIsInitialized() const {
	return sdl_init_;
}

bool SDLInstance::SDLImageIsInitialized() const {
	return sdl_image_init_;
}

void SDLInstance::Init() {
#ifdef PTGN_DEBUG
	PTGN_INFO("Build Type: Debug");
#else
	PTGN_INFO("Build Type: Release");
#endif
	PTGN_ASSERT(!IsInitialized());
	InitSDL();
	InitSDLImage();
	InitSDLTTF();
	InitSDLMixer();
	PTGN_ASSERT(IsInitialized());
}

void SDLInstance::Shutdown() {
	Mix_CloseAudio();
	PTGN_INFO("Closed SDL2_mixer audio");
	Mix_Quit();
	PTGN_INFO("Deinitialized SDL2_mixer");
	sdl_mixer_init_ = false;
	TTF_Quit();
	PTGN_INFO("Deinitialized SDL2_ttf");
	sdl_ttf_init_ = false;
	IMG_Quit();
	PTGN_INFO("Deinitialized SDL2_image");
	sdl_image_init_ = false;
	SDL_Quit();
	PTGN_INFO("Deinitialized SDL2");
	sdl_init_ = false;
}

void SDLInstance::Delay(milliseconds time) {
	std::this_thread::sleep_for(time);
	/*SDL_Delay(std::chrono::duration_cast<duration<std::uint32_t,
	milliseconds::period>>(time).count(
	));*/
}

void SDLInstance::InitSDL() {
	std::uint32_t sdl_flags{ SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_TIMER };
	PTGN_ASSERT(
		SDL_WasInit(sdl_flags) != sdl_flags, "Cannot reinitialize SDL instance before shutting down"
	);

	// Ensures window and elements scale by monitor zoom level for constant
	// appearance.
	SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");

	int sdl_init{ SDL_Init(sdl_flags) };
	PTGN_ASSERT(sdl_init == 0, SDL_GetError());

	SDL_version linked;
	SDL_GetVersion(&linked);
	PTGN_INFO("Initialized SDL2 version: ", linked);

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, PTGN_OPENGL_CONTEXT_PROFILE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, PTGN_OPENGL_MAJOR_VERSION);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, PTGN_OPENGL_MINOR_VERSION);

	sdl_init_ = true;
}

void SDLInstance::InitSDLImage() {
	int img_flags{ IMG_INIT_PNG | IMG_INIT_JPG };

	PTGN_ASSERT(
		IMG_Init(0) != img_flags, "Cannot reinitialize SDL_image instance before shutting down"
	);

	int img_init{ IMG_Init(img_flags) };

	PTGN_ASSERT(img_init == img_flags, IMG_GetError());

	const SDL_version* linked = IMG_Linked_Version();
	PTGN_INFO("Initialized SDL2_image version: ", *linked);

	sdl_image_init_ = true;
}

void SDLInstance::InitSDLTTF() {
	PTGN_ASSERT(TTF_WasInit() == 0, "Cannot reinitialize SDL_ttf instance before shutting down");

	int ttf_init{ TTF_Init() };
	PTGN_ASSERT(ttf_init != -1, TTF_GetError());

	const SDL_version* linked = TTF_Linked_Version();
	PTGN_INFO("Initialized SDL2_ttf version: ", *linked);

	sdl_ttf_init_ = true;
}

void SDLInstance::InitSDLMixer() {
	int mixer_flags{ MIX_INIT_MP3 | MIX_INIT_OGG | MIX_INIT_OPUS |
					 MIX_INIT_WAVPACK /* | MIX_INIT_FLAC | MIX_INIT_MOD | MIX_INIT_MID*/ };

#ifdef __EMSCRIPTEN__
	mixer_flags = { MIX_INIT_OGG };
#endif

	PTGN_ASSERT(
		Mix_Init(0) != mixer_flags, "Cannot reinitialize SDL_mixer instance before shutting down"
	);

	if (int mixer_init{ Mix_Init(mixer_flags) }; mixer_init != mixer_flags) {
		PTGN_WARN(Mix_GetError());
	}

	int audio_open{ Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) };
	PTGN_ASSERT(audio_open != -1, Mix_GetError());

	const SDL_version* linked = Mix_Linked_Version();
	PTGN_INFO("Initialized SDL2_mixer version: ", *linked);

	sdl_mixer_init_ = true;
}

} // namespace ptgn::impl
