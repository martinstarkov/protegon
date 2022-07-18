#include "SoundManager.h"

#include <SDL_mixer.h>

namespace ptgn {

void SoundManager::Pause(int channel) {
	Mix_Pause(channel);
}

void SoundManager::Resume(int channel) {
	Mix_Resume(channel);
}

void SoundManager::Stop(int channel) {
	Mix_HaltChannel(channel);
}

void SoundManager::FadeOut(int channel, milliseconds time) {
	Mix_FadeOutChannel(channel, time.count());
}

bool SoundManager::IsPlaying(int channel) {
	return Mix_Playing(channel);
}

bool SoundManager::IsPaused(int channel) {
	return Mix_Paused(channel);
}

bool SoundManager::IsFading(int channel) {
	switch (Mix_FadingChannel(channel)) {
		case MIX_NO_FADING:  return false;
		case MIX_FADING_OUT: return true;
		case MIX_FADING_IN:  return true;
		default:             return false;
	}
}

} // namespace ptgn