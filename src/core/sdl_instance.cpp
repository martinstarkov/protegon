#include "sdl_instance.h"

#include <ostream>

#include "SDL.h"
#include "SDL_image.h"
#include "SDL_mixer.h"
#include "SDL_ttf.h"
#include "renderer/renderer.h"
#include "utility/debug.h"

inline std::ostream& operator<<(std::ostream& os, const SDL_version& v) {
	os << static_cast<int>(v.major) << "." << static_cast<int>(v.minor) << "."
	   << static_cast<int>(v.patch);
	return os;
}

namespace ptgn {

namespace impl {

SDLInstance::SDLInstance() {
	InitSDL();
	InitSDLImage();
	InitSDLTTF();
	InitSDLMixer();
}

SDLInstance::~SDLInstance() {
	Mix_CloseAudio();
	PTGN_INFO("Closed SDL2_mixer audio");
	Mix_Quit();
	PTGN_INFO("Deinitialized SDL2_mixer");
	TTF_Quit();
	PTGN_INFO("Deinitialized SDL2_ttf");
	IMG_Quit();
	PTGN_INFO("Deinitialized SDL2_image");
	SDL_Quit();
	PTGN_INFO("Deinitialized SDL2");
}

void SDLInstance::InitSDL() {
	std::uint32_t sdl_flags{ SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_TIMER | SDL_INIT_VIDEO |
							 SDL_VIDEO_OPENGL };
	if (SDL_WasInit(sdl_flags) == sdl_flags) {
		// TODO: Temporary message.
		PTGN_INFO("Not going to try to initialize SDL2 again");
		return;
	}

	SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_SCALING, "1");
	// Ensures window and elements scale by monitor zoom level for constant
	// appearance.
	SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
	SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
	SDL_SetHint(SDL_HINT_RENDER_BATCHING, "1");

	int sdl_init{ SDL_Init(sdl_flags) };
	PTGN_ASSERT(sdl_init == 0, SDL_GetError());

	SDL_version linked;
	SDL_GetVersion(&linked);
	PTGN_INFO("Initialized SDL2 version: ", linked);

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, PTGN_OPENGL_MAJOR_VERSION);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, PTGN_OPENGL_MINOR_VERSION);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
}

void SDLInstance::InitSDLImage() {
	int img_flags{ IMG_INIT_PNG | IMG_INIT_JPG };

	if (IMG_Init(0) == img_flags) {
		// TODO: Temporary message.
		PTGN_INFO("Not going to try to initialize SDL2_image again");
		return;
	}

	int img_init{ IMG_Init(img_flags) };

	PTGN_ASSERT(img_init == img_flags, IMG_GetError());

	const SDL_version* linked = IMG_Linked_Version();
	PTGN_INFO("Initialized SDL2_image version: ", *linked);
}

void SDLInstance::InitSDLTTF() {
	if (TTF_WasInit()) {
		// TODO: Temporary message.
		PTGN_INFO("Not going to try to initialize SDL2_ttf again");
		return;
	}

	int ttf_init{ TTF_Init() };
	PTGN_ASSERT(ttf_init != -1, TTF_GetError());

	const SDL_version* linked = TTF_Linked_Version();
	PTGN_INFO("Initialized SDL2_ttf version: ", *linked);
}

void SDLInstance::InitSDLMixer() {
	int mixer_flags{ MIX_INIT_MP3 | MIX_INIT_OGG | MIX_INIT_OPUS |
					 MIX_INIT_WAVPACK /* | MIX_INIT_FLAC | MIX_INIT_MOD | MIX_INIT_MID*/ };

	if (Mix_Init(0) == mixer_flags) {
		// TODO: Temporary message.
		PTGN_INFO("Not going to try to initialize SDL2_mixer again");
		return;
	}

	int mixer_init{ Mix_Init(mixer_flags) };
	PTGN_ASSERT(mixer_init == mixer_flags, Mix_GetError());

	int audio_open{ Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) };
	PTGN_ASSERT(audio_open != -1, Mix_GetError());

	const SDL_version* linked = Mix_Linked_Version();
	PTGN_INFO("Initialized SDL2_mixer version: ", *linked);
}

} // namespace impl

} // namespace ptgn