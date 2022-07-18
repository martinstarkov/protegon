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
	if (!Exists()) {
		PrintLine(Mix_GetError());
		assert(!"Failed to load music into sound manager");
	}
}

Music::~Music() {
	Mix_FreeMusic(music_);
	music_ = nullptr;
}

void Music::Play(int loops) const {
	assert(Exists() && "Cannot play nonexistent music");
	Mix_PlayMusic(music_, loops);
}

void Music::FadeIn(int loops, milliseconds time) const {
	assert(Exists() && "Cannot fade in nonexistent music");
	Mix_FadeInMusic(music_, loops, time.count());
}

} // namespace ptgn