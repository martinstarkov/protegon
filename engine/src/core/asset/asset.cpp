#include "core/asset/asset.h"

#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_ttf/SDL_ttf.h>

namespace ptgn::impl {

void MIX_AudioDeleter::operator()(MIX_Audio* audio) const {
	MIX_DestroyAudio(audio);
}

void TTF_FontDeleter::operator()(TTF_Font* font) const {
	TTF_CloseFont(font);
}

} // namespace ptgn::impl