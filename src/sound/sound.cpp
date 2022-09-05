#include "protegon/sound.h"

#include <cassert> // assert

#include <SDL_mixer.h>

#include "protegon/log.h"
#include "utility/file.h"

namespace ptgn {

void Music::Mix_Music_Deleter::operator()(Mix_Music* music) {
	Mix_FreeMusic(music);
}

Music::Music(const char* music_path) {
	assert(music_path != "" && "Empty music path?");
	assert(FileExists(music_path) && "Nonexistent music file path?");
	music_ = std::make_shared<Mix_Music>(Mix_LoadMUS(music_path), Mix_Music_Deleter{});
	if (!IsValid()) {
		PrintLine(Mix_GetError());
		assert(!"Failed to create music");
	}
}

bool Music::IsValid() const {
	return music_ != nullptr;
}

void Music::Play(int loops) const {
	assert(IsValid() && "Cannot play nonexistent music");
	Mix_PlayMusic(music_.get(), loops);
}

void Music::FadeIn(int loops, milliseconds time) const {
	assert(IsValid() && "Cannot fade in nonexistent music");
	Mix_FadeInMusic(music_.get(), loops, time.count());
}

void Sound::Mix_Chunk_Deleter::operator()(Mix_Chunk* chunk) {
	Mix_FreeChunk(chunk);
}

Sound::Sound(const char* sound_path) {
	assert(sound_path != "" && "Empty sound path?");
	assert(FileExists(sound_path) && "Nonexistent sound file path?");
	chunk_ = std::make_shared<Mix_Chunk>(Mix_LoadWAV(sound_path), Mix_Chunk_Deleter{});
	if (!IsValid()) {
		PrintLine(Mix_GetError());
		assert(!"Failed to create sound chunk");
	}
}

void Sound::Play(int channel, int loops) const {
	assert(IsValid() && "Cannot play nonexistent sound");
	Mix_PlayChannel(channel, chunk_.get(), loops);
}

void Sound::FadeIn(int channel, int loops, milliseconds time) const {
	assert(IsValid() && "Cannot fade in nonexistent sound");
	Mix_FadeInChannel(channel, chunk_.get(), loops, time.count());
}

bool Sound::IsValid() const {
	return chunk_ != nullptr;
}

} // namespace ptgn