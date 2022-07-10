#include "Music.h"

#include <cassert> // assert

#include <SDL_mixer.h>

#include "utility/Log.h"
#include "utility/File.h"

namespace ptgn {

Music::Music(const char* music_path) {
	assert(music_path != "" && "Cannot load empty music path into the music manager");
	assert(FileExists(music_path) && "Cannot load music with nonexistent file path into the music manager");
	music_ = Mix_LoadMUS(music_path);
	if (music_ == NULL) {
		PrintLine("Failed to load music into sound manager: ", Mix_GetError());
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

} // namespace ptgn