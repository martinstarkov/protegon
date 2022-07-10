#include "Sound.h"

#include <cassert> // assert

#include <SDL_mixer.h>

#include "utility/Log.h"
#include "utility/File.h"

namespace ptgn {

Sound::Sound(const char* sound_path) {
	assert(sound_path != "" && "Cannot load empty sound path into the sound manager");
	assert(FileExists(sound_path) && "Cannot load sound with nonexistent file path into the sound manager");
	chunk_ = Mix_LoadWAV(sound_path);
	if (!Exists()) {
		PrintLine(Mix_GetError());
		assert(!"Failed to load sound into the sound manager");
	}
}

Sound::~Sound() {
	Mix_FreeChunk(chunk_);
	chunk_ = nullptr;
}

void Sound::Play(int channel, int loops) const {
	assert(Exists() && "Cannot play nonexistent sound");
	Mix_PlayChannel(channel, chunk_, loops);
}

void Sound::FadeIn(int channel, int loops, milliseconds time) const {
	assert(Exists() && "Cannot fade in nonexistent sound");
	Mix_FadeInChannel(channel, chunk_, loops, time.count());
}

} // namespace ptgn