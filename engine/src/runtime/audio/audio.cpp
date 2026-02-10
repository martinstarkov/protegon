// #include "audio/audio.h"
//
// #include <chrono>
// #include <filesystem>
// #include <memory>
// #include <ratio>
// #include <string>
//
// #include "SDL_mixer.h"
// #include "core/assert.h"
// #include "core/asset/asset_handle.h"
// #include "core/asset/asset_manager.h"
// #include "ecs/components/generic.h"
// #include "core/util/file.h"
// #include "core/util/time.h"
//
// namespace ptgn {
//
// namespace music {
//
// Music LoadFromFile(const path& filepath) {}
//
// void Play(const ResourceHandle& key, int loops) const {
//	Mix_PlayMusic(Get(key).get(), loops);
// }
//
// void FadeIn(const ResourceHandle& key, milliseconds fade_time, int loops) const {
//	auto time_int{ to_duration<duration<int, milliseconds::period>>(fade_time) };
//	Mix_FadeInMusic(Get(key).get(), loops, time_int.count());
// }
//
// void Stop() const {
//	Mix_HaltMusic();
// }
//
// void FadeOut(milliseconds time) const {
//	auto time_int{ to_duration<duration<int, std::milli>>(time) };
//	Mix_FadeOutMusic(time_int.count());
// }
//
// void Pause() const {
//	Mix_PauseMusic();
// }
//
// void Resume() const {
//	Mix_ResumeMusic();
// }
//
// void ToggleVolume(int new_volume) const {
//	if (GetVolume() != 0) {
//		SetVolume(0);
//	} else {
//		SetVolume(new_volume);
//	}
// }
//
// void TogglePause() const {
//	if (IsPaused()) {
//		Resume();
//	} else {
//		Pause();
//	}
// }
//
// int GetVolume() const {
//	return Mix_VolumeMusic(-1);
// }
//
// void SetVolume(int volume) const {
//	PTGN_ASSERT(
//		volume >= 0 && volume <= max_volume, "Cannot set music volume outside of valid range"
//	);
//	Mix_VolumeMusic(volume);
// }
//
// bool IsPlaying() const {
//	return static_cast<bool>(Mix_PlayingMusic());
// }
//
// bool IsPaused() const {
//	return static_cast<bool>(Mix_PausedMusic());
// }
//
// bool IsFading() const {
//	switch (Mix_FadingMusic()) {
//		case MIX_NO_FADING:	 return false;
//		case MIX_FADING_OUT: return true;
//		case MIX_FADING_IN:	 return true;
//		default:			 return false;
//	}
// }
//
// } // namespace music
//
// namespace sound {
//
// void Play(const ResourceHandle& key, int channel, int loops) const {
//	PTGN_ASSERT(Has(key), "Cannot play sound which has not been loaded in the music manager");
//	Mix_PlayChannel(channel, Get(key).get(), loops);
// }
//
// void FadeIn(const ResourceHandle& key, milliseconds fade_time, int channel, int loops) const {
//	PTGN_ASSERT(Has(key), "Cannot fade in sound which has not been loaded in the music manager");
//	auto time_int{ to_duration<duration<int, std::milli>>(fade_time) };
//	Mix_FadeInChannel(channel, Get(key).get(), loops, time_int.count());
// }
//
// void SetVolume(const ResourceHandle& key, int volume) const {
//	PTGN_ASSERT(
//		Has(key), "Cannot set volume of sound which has not been loaded in the music manager"
//	);
//	PTGN_ASSERT(
//		volume >= 0 && volume <= max_volume, "Cannot set sound volume outside of valid range"
//	);
//	Mix_VolumeChunk(Get(key).get(), volume);
// }
//
// void SetVolume(int channel, int volume) const {
//	PTGN_ASSERT(
//		volume >= 0 && volume <= max_volume,
//		"Cannot set sound channel volume outside of valid range"
//	);
//	Mix_Volume(channel, volume);
// }
//
// int GetVolume(const ResourceHandle& key) const {
//	PTGN_ASSERT(
//		Has(key), "Cannot get volume of sound which has not been loaded in the music manager"
//	);
//	return Mix_VolumeChunk(Get(key).get(), -1);
// }
//
// void ToggleVolume(const ResourceHandle& key, int new_volume) const {
//	PTGN_ASSERT(
//		Has(key), "Cannot toggle volume of sound which has not been loaded in the music manager"
//	);
//	if (GetVolume(key) != 0) {
//		SetVolume(key, 0);
//	} else {
//		SetVolume(key, new_volume);
//	}
// }
//
// void Stop(int channel) const {
//	Mix_HaltChannel(channel);
// }
//
// void Resume(int channel) const {
//	Mix_Resume(channel);
// }
//
// void Pause(int channel) const {
//	Mix_Pause(channel);
// }
//
// void TogglePause(int channel) const {
//	if (IsPaused(channel)) {
//		Resume(channel);
//	} else {
//		Pause(channel);
//	}
// }
//
// void FadeOut(milliseconds fade_time, int channel) const {
//	auto time_int{ to_duration<duration<int, std::milli>>(fade_time) };
//	Mix_FadeOutChannel(channel, time_int.count());
// }
//
// int GetVolume(int channel) const {
//	return Mix_Volume(channel, -1);
// }
//
// bool IsPlaying(int channel) const {
//	return static_cast<bool>(Mix_Playing(channel));
// }
//
// bool IsPaused(int channel) const {
//	return static_cast<bool>(Mix_Paused(channel));
// }
//
// bool IsFading(int channel) const {
//	switch (Mix_FadingChannel(channel)) {
//		case MIX_NO_FADING:	 return false;
//		case MIX_FADING_OUT: return true;
//		case MIX_FADING_IN:	 return true;
//		default:			 return false;
//	}
// }
//
// } // namespace sound
//
// } // namespace ptgn