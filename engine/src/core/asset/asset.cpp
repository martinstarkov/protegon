#include "core/asset/asset.h"

#include "SDL_mixer.h"
#include "SDL_ttf.h"

namespace ptgn {

namespace impl {

void Mix_MusicDeleter::operator()(Mix_Music* music) const {
	Mix_FreeMusic(music);
}

void Mix_ChunkDeleter::operator()(Mix_Chunk* sound) const {
	Mix_FreeChunk(sound);
}

void TTF_FontDeleter::operator()(TTF_Font* font) const {
	TTF_CloseFont(font);
}

} // namespace impl

} // namespace ptgn