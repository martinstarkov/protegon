#include "MusicManager.h"

#include <SDL_mixer.h>

namespace ptgn {

void MusicManager::Stop() {
	Mix_HaltMusic();
}

void MusicManager::FadeOut(milliseconds time) {
	Mix_FadeOutMusic(time.count());
}

void MusicManager::Pause() {
	Mix_PauseMusic();
}

void MusicManager::Resume() {
	Mix_ResumeMusic();
}

bool MusicManager::IsPlaying() {
	return Mix_PlayingMusic();
}

bool MusicManager::IsPaused() {
	return Mix_PausedMusic();
}

bool MusicManager::IsFading() {
	switch (Mix_FadingMusic()) {
		case MIX_NO_FADING:  return false;
		case MIX_FADING_OUT: return true;
		case MIX_FADING_IN:  return true;
		default:             return false;
	}
}

} // namespace ptgn