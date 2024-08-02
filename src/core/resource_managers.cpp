#include "resource_managers.h"

#include "SDL_mixer.h"

namespace ptgn {

void MusicManager::Stop() {
	Mix_HaltMusic();
}

void MusicManager::FadeOut(milliseconds time) {
	auto time_int =
		std::chrono::duration_cast<duration<int, std::milli>>(time
		);
	Mix_FadeOutMusic(time_int.count());
}

void MusicManager::Pause() {
	Mix_PauseMusic();
}

void MusicManager::Resume() {
	Mix_ResumeMusic();
}

void MusicManager::Toggle(int optional_new_volume) {
	if (GetVolume() != 0) {
		Mute();
	} else {
		Unmute(optional_new_volume);
	}
}

int MusicManager::GetVolume() {
	return Mix_VolumeMusic(-1);
}

void MusicManager::SetVolume(int new_volume) {
	Mix_VolumeMusic(new_volume);
}

void MusicManager::Mute() {
	SetVolume(0);
}

void MusicManager::Unmute(int optional_new_volume) {
	if (optional_new_volume == -1) {
		SetVolume(MIX_MAX_VOLUME);
		return;
	}
	PTGN_ASSERT(optional_new_volume >= 0, "Cannot unmute to volume below 0");
	PTGN_ASSERT(
		optional_new_volume <= MIX_MAX_VOLUME,
		"Cannot unmute to volume above max volume (128)"
	);
	SetVolume(optional_new_volume);
}

bool MusicManager::IsPlaying() {
	return Mix_PlayingMusic();
}

bool MusicManager::IsPaused() {
	return Mix_PausedMusic();
}

bool MusicManager::IsFading() {
	switch (Mix_FadingMusic()) {
		case MIX_NO_FADING:	 return false;
		case MIX_FADING_OUT: return true;
		case MIX_FADING_IN:	 return true;
		default:			 return false;
	}
}

void SoundManager::HaltChannel(int channel) {
	Mix_HaltChannel(channel);
}

void SoundManager::ResumeChannel(int channel) {
	Mix_Resume(channel);
}

} // namespace ptgn