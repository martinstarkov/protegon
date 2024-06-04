#include "protegon/sound.h"

#include <cassert>

#include <SDL_mixer.h>

#include "protegon/log.h"

namespace ptgn {

Music::Music(const path& music_path) {
	assert(FileExists(music_path) && "Cannot create music from a nonexistent music path");
	instance_ = { Mix_LoadMUS(music_path.string().c_str()), Mix_FreeMusic };
	if (!IsValid()) {
		PrintLine(Mix_GetError());
		assert(!"Failed to create music");
	}
}

void Music::Play(int loops) const {
	assert(IsValid() && "Cannot play uninitialized or destroyed music");
	Mix_PlayMusic(instance_.get(), loops);
}

void Music::FadeIn(int loops, milliseconds time) const {
	assert(IsValid() && "Cannot fade in uninitialized or destroyed music");
    const auto time_int = std::chrono::duration_cast<std::chrono::duration<int, milliseconds::period>>(time);
	Mix_FadeInMusic(instance_.get(), loops, time_int.count());
}

Sound::Sound(const path& sound_path) {
	assert(FileExists(sound_path) && "Cannot create sound from a nonexistent sound path");
	instance_ = { Mix_LoadWAV(sound_path.string().c_str()), Mix_FreeChunk };
	if (!IsValid()) {
		PrintLine(Mix_GetError());
		assert(!"Failed to create sound");
	}
}

void Sound::Play(int channel, int loops) const {
	assert(IsValid() && "Cannot play uninitialized or destroyed sound");
	Mix_PlayChannel(channel, instance_.get(), loops);
}

void Sound::FadeIn(int channel, int loops, milliseconds time) const {
	assert(IsValid() && "Cannot fade in uninitialized or destroyed sound");
    const auto time_int = std::chrono::duration_cast<std::chrono::duration<int, std::milli>>(time);
	Mix_FadeInChannel(channel, instance_.get(), loops, time_int.count());
}

} // namespace ptgn