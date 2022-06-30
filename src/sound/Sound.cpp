#include "Sound.h"

#include "debugging/Debug.h"

#include <SDL_mixer.h>

namespace ptgn {

namespace internal {

Sound::Sound(const char* sound_path) {
	assert(sound_path != "" && "Cannot load empty sound path into the sound manager");
	assert(debug::FileExists(sound_path) && "Cannot load sound with non-existent file path into the sound manager");
	chunk_ = Mix_LoadWAV(sound_path);
	if (chunk_ == NULL) {
		debug::PrintLine(Mix_GetError());
		assert(!"Failed to load sound into the sound manager");
	}
}

Sound::~Sound() {
	Mix_FreeChunk(chunk_);
	chunk_ = nullptr;
}

void Sound::Play(int channel, int loops) const {
	Mix_PlayChannel(channel, chunk_, loops);
}

void Sound::Pause(int channel) const {
	Mix_Pause(channel);
}

void Sound::Resume(int channel) const {
	Mix_Resume(channel);
}

void Sound::Stop(int channel) const {
	Mix_HaltChannel(channel);
}

void Sound::FadeOut(int channel, milliseconds time) const {
	Mix_FadeOutChannel(channel, time.count());
}

void Sound::FadeIn(int channel, int loops, milliseconds time) const {
	Mix_FadeInChannel(channel, chunk_, loops, time.count());
}

bool Sound::IsPlaying(int channel) const {
	return Mix_Playing(channel);
}

bool Sound::IsPaused(int channel) const {
	return Mix_Paused(channel);
}

bool Sound::IsFading(int channel) const {
	switch (Mix_FadingChannel(channel)) {
		case MIX_NO_FADING:  return false;
		case MIX_FADING_OUT: return true;
		case MIX_FADING_IN:  return true;
		default:             return false;
	}
}

Sound::operator Mix_Chunk*() const {
	return chunk_;
}

} // namespace internal

} // namespace ptgn