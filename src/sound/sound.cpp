#include "protegon/sound.h"

#include <cassert> // assert

#include <SDL_mixer.h>

#include "protegon/log.h"
#include "utility/file.h"

namespace ptgn {

Music::Music(const char* music_path) {
	assert(music_path != "" && "Empty music path?");
	assert(FileExists(music_path) && "Nonexistent music file path?");
	music_ = Mix_LoadMUS(music_path);
	if (!IsValid()) {
		PrintLine(Mix_GetError());
		assert(!"Failed to create music");
	}
}

Music::~Music() {
	Mix_FreeMusic(music_);
	music_ = nullptr;
}

bool Music::IsValid() const {
	return music_ != nullptr;
}

void Music::Play(int loops) const {
	assert(IsValid() && "Cannot play nonexistent music");
	Mix_PlayMusic(music_, loops);
}

void Music::FadeIn(int loops, milliseconds time) const {
	assert(IsValid() && "Cannot fade in nonexistent music");
	Mix_FadeInMusic(music_, loops, time.count());
}

Sound::Sound(const char* sound_path) {
	assert(sound_path != "" && "Empty sound path?");
	assert(FileExists(sound_path) && "Nonexistent sound file path?");
	chunk_ = Mix_LoadWAV(sound_path);
	if (!IsValid()) {
		PrintLine(Mix_GetError());
		assert(!"Failed to create sound chunk");
	}
}

Sound::~Sound() {
	Mix_FreeChunk(chunk_);
	chunk_ = nullptr;
}

void Sound::Play(int channel, int loops) const {
	assert(IsValid() && "Cannot play nonexistent sound");
	Mix_PlayChannel(channel, chunk_, loops);
}

void Sound::FadeIn(int channel, int loops, milliseconds time) const {
	assert(IsValid() && "Cannot fade in nonexistent sound");
	Mix_FadeInChannel(channel, chunk_, loops, time.count());
}

bool Sound::IsValid() const {
	return chunk_ != nullptr;
}

} // namespace ptgn