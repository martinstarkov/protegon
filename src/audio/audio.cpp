#include "protegon/audio.h"

#include "SDL_mixer.h"
#include "utility/debug.h"

namespace ptgn {

Music::Music(const path& music_path) {
	PTGN_ASSERT(FileExists(music_path), "Cannot create music from a nonexistent music path");
	instance_ = { Mix_LoadMUS(music_path.string().c_str()), Mix_FreeMusic };
	PTGN_ASSERT(IsValid(), Mix_GetError());
}

void Music::Play(int loops) const {
	PTGN_ASSERT(IsValid(), "Cannot play uninitialized or destroyed music");
	Mix_PlayMusic(instance_.get(), loops);
}

void Music::FadeIn(int loops, milliseconds time) const {
	PTGN_ASSERT(IsValid(), "Cannot fade in uninitialized or destroyed music");
	const auto time_int = std::chrono::duration_cast<duration<int, milliseconds::period>>(time);
	Mix_FadeInMusic(instance_.get(), loops, time_int.count());
}

Sound::Sound(const path& sound_path) {
	PTGN_ASSERT(FileExists(sound_path), "Cannot create sound from a nonexistent sound path");
	instance_ = { Mix_LoadWAV(sound_path.string().c_str()), Mix_FreeChunk };
	PTGN_ASSERT(IsValid(), Mix_GetError());
}

void Sound::Play(int channel, int loops) const {
	PTGN_ASSERT(IsValid(), "Cannot play uninitialized or destroyed sound");
	Mix_PlayChannel(channel, instance_.get(), loops);
}

void Sound::FadeIn(int channel, int loops, milliseconds time) const {
	PTGN_ASSERT(IsValid(), "Cannot fade in uninitialized or destroyed sound");
	const auto time_int = std::chrono::duration_cast<duration<int, std::milli>>(time);
	Mix_FadeInChannel(channel, instance_.get(), loops, time_int.count());
}

} // namespace ptgn