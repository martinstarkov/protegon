#include "Music.h"

#include <SDL_mixer.h>

#include "debugging/Debug.h"

namespace ptgn {

namespace internal {

Music::Music(const char* music_path) {
	assert(music_path != "" && "Cannot load empty music path into the music manager");
	assert(debug::FileExists(music_path) && "Cannot load music with non-existent file path into the music manager");
	music_ = Mix_LoadMUS(music_path);
	if (music_ == NULL) {
		debug::PrintLine("Failed to load music into sound manager: ", Mix_GetError());
	}
}

Music::~Music() {
	Mix_FreeMusic(music_);
	music_ = nullptr;
}

void Music::Play(int loops) const {
	Mix_PlayMusic(music_, loops);
}

void Music::FadeIn(int loops, milliseconds time) const {
	Mix_FadeInMusic(music_, loops, time.count());
}

Music::operator Mix_Music*() const {
	return music_;
}

} // namespace internal

} // namespace ptgn