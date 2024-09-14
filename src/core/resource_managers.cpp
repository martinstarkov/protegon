#include "core/resource_managers.h"

#include <chrono>
#include <list>
#include <ratio>
#include <utility>

#include "core/manager.h"
#include "protegon/tween.h"
#include "SDL_mixer.h"
#include "utility/debug.h"
#include "utility/time.h"

namespace ptgn {

void TweenManager::Update(float dt) {
	auto& m{ GetMap() };

	for (auto it = m.begin(); it != m.end();) {
		const auto& key{ it->first };
		auto& tween{ it->second };
		// TODO: Figure out how to do timestep accumulation outside of tweens, using
		// StepImpl(dt, false) and some added logic outside of this loop. This is important
		// because currently tween internal timestep accumulation causes all callbacks to be
		// triggered sequentially for each tween before moving onto the next tween. Desired
		// callback behavior:
		// 1. Tween1Repeat#1 2. Tween2Repeat#1 3. Tween1Repeat#2 4. Tween2Repeat#2.
		// Current callback behavior:
		// 1. Tween1Repeat#1 2. Tween1Repeat#2 3. Tween2Repeat#1 4. Tween2Repeat#2.

		tween.Step(dt);

		if (tween.IsCompleted() && keep_alive_tweens_.count(key) == 0) {
			it = m.erase(it);
		} else {
			++it;
		}
	}
}

void MusicManager::Stop() const {
	Mix_HaltMusic();
}

void MusicManager::FadeOut(milliseconds time) const {
	auto time_int = std::chrono::duration_cast<duration<int, std::milli>>(time);
	Mix_FadeOutMusic(time_int.count());
}

void MusicManager::Pause() const {
	Mix_PauseMusic();
}

void MusicManager::Resume() const {
	Mix_ResumeMusic();
}

void MusicManager::Toggle(int optional_new_volume) const {
	if (GetVolume() != 0) {
		Mute();
	} else {
		Unmute(optional_new_volume);
	}
}

int MusicManager::GetVolume() const {
	return Mix_VolumeMusic(-1);
}

void MusicManager::SetVolume(int new_volume) const {
	Mix_VolumeMusic(new_volume);
}

void MusicManager::Mute() const {
	SetVolume(0);
}

void MusicManager::Unmute(int optional_new_volume) const {
	if (optional_new_volume == -1) {
		SetVolume(MIX_MAX_VOLUME);
		return;
	}
	PTGN_ASSERT(optional_new_volume >= 0, "Cannot unmute to volume below 0");
	PTGN_ASSERT(
		optional_new_volume <= MIX_MAX_VOLUME, "Cannot unmute to volume above max volume (128)"
	);
	SetVolume(optional_new_volume);
}

bool MusicManager::IsPlaying() const {
	return Mix_PlayingMusic();
}

bool MusicManager::IsPaused() const {
	return Mix_PausedMusic();
}

bool MusicManager::IsFading() const {
	switch (Mix_FadingMusic()) {
		case MIX_NO_FADING:	 return false;
		case MIX_FADING_OUT: return true;
		case MIX_FADING_IN:	 return true;
		default:			 return false;
	}
}

void SoundManager::HaltChannel(int channel) const {
	Mix_HaltChannel(channel);
}

void SoundManager::ResumeChannel(int channel) const {
	Mix_Resume(channel);
}

void SoundManager::FadeOut(int channel, milliseconds time) const {
	const auto time_int = std::chrono::duration_cast<duration<int, std::milli>>(time);
	Mix_FadeOutChannel(channel, time_int.count());
}

} // namespace ptgn