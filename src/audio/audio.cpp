#include "audio/audio.h"

#include <chrono>
#include <filesystem>
#include <ratio>
#include <string>

#include "core/game.h"
#include "core/manager.h"
#include "core/sdl_instance.h"
#include "SDL_mixer.h"
#include "utility/debug.h"
#include "utility/file.h"
#include "utility/handle.h"
#include "utility/time.h"
#include "audio.h"

namespace ptgn {

namespace impl {

struct MixMusicDeleter {
	void operator()(Mix_Music* music) const {
		if (game.sdl_instance_->SDLMixerIsInitialized()) {
			Mix_FreeMusic(music);
		}
	}
};

struct MixChunkDeleter {
	void operator()(Mix_Chunk* sound) const {
		if (game.sdl_instance_->SDLMixerIsInitialized()) {
			Mix_FreeChunk(sound);
		}
	}
};

} // namespace impl

Music::Music(const path& music_path) {
	PTGN_ASSERT(
		FileExists(music_path),
		"Cannot create music from a nonexistent music path: ", music_path.string()
	);
	Create({ Mix_LoadMUS(music_path.string().c_str()), impl::MixMusicDeleter{} });
	PTGN_ASSERT(IsValid(), Mix_GetError());
}

void Music::Play(int loops) {
	Mix_PlayMusic(&Get(), loops);
}

void Music::FadeIn(milliseconds fade_time, int loops) {
	auto time_int{ std::chrono::duration_cast<duration<int, milliseconds::period>>(fade_time) };
	Mix_FadeInMusic(&Get(), loops, time_int.count());
}

Sound::Sound(const path& sound_path) {
	PTGN_ASSERT(
		FileExists(sound_path),
		"Cannot create sound from a nonexistent sound path: ", sound_path.string()
	);
	Create({ Mix_LoadWAV(sound_path.string().c_str()), impl::MixChunkDeleter{} });
	PTGN_ASSERT(IsValid(), Mix_GetError());
}

void Sound::Play(int channel, int loops) {
	Mix_PlayChannel(channel, &Get(), loops);
}

void Sound::FadeIn(milliseconds fade_time, int channel, int loops) {
	auto time_int{ std::chrono::duration_cast<duration<int, std::milli>>(fade_time) };
	Mix_FadeInChannel(channel, &Get(), loops, time_int.count());
}

void Sound::SetVolume(int volume) {
	PTGN_ASSERT(volume >= 0 && volume <= max_volume, "Cannot set sound volume outside of valid range");
	Mix_VolumeChunk(&Get(), volume);
}

int Sound::GetVolume() const {
	return Mix_VolumeChunk(const_cast<Mix_Chunk*>(&Get()), -1);
}

void Sound::Mute() {
	SetVolume(0);
}

void Sound::Unmute(int new_volume) {
	SetVolume(new_volume);
}

void Sound::ToggleMute(int new_volume) {
	if (GetVolume() != 0) {
		Mute();
	} else {
		Unmute(new_volume);
	}
}

namespace impl {

void MusicManager::Stop() const {
	Mix_HaltMusic();
}

void MusicManager::FadeOut(milliseconds time) const {
	auto time_int{ std::chrono::duration_cast<duration<int, std::milli>>(time) };
	Mix_FadeOutMusic(time_int.count());
}

void MusicManager::Pause() const {
	Mix_PauseMusic();
}

void MusicManager::Resume() const {
	Mix_ResumeMusic();
}

void MusicManager::ToggleMute(int new_volume) const {
	if (GetVolume() != 0) {
		Mute();
	} else {
		Unmute(new_volume);
	}
}

void MusicManager::TogglePause() const {
	if (IsPaused()) {
		Resume();
	} else {
		Pause();
	}
}

int MusicManager::GetVolume() const {
	return Mix_VolumeMusic(-1);
}

void MusicManager::SetVolume(int volume) const {
	PTGN_ASSERT(volume >= 0 && volume <= max_volume, "Cannot set music volume outside of valid range");
	Mix_VolumeMusic(volume);
}

void MusicManager::Mute() const {
	SetVolume(0);
}

void MusicManager::Unmute(int new_volume) const {
	SetVolume(new_volume);
}

bool MusicManager::IsPlaying() const {
	return static_cast<bool>(Mix_PlayingMusic());
}

bool MusicManager::IsPaused() const {
	return static_cast<bool>(Mix_PausedMusic());
}

bool MusicManager::IsFading() const {
	switch (Mix_FadingMusic()) {
		case MIX_NO_FADING:	 return false;
		case MIX_FADING_OUT: return true;
		case MIX_FADING_IN:	 return true;
		default:			 return false;
	}
}

void MusicManager::Reset() {
	Stop();
	MapManager::Reset();
}

void SoundManager::Stop(int channel) const {
	Mix_HaltChannel(channel);
}

void SoundManager::Resume(int channel) const {
	Mix_Resume(channel);
}

void SoundManager::Pause(int channel) const {
	Mix_Pause(channel);
}

void SoundManager::TogglePause(int channel) const {
	if (IsPaused(channel)) {
		Resume(channel);
	} else {
		Pause(channel);
	}
}

void SoundManager::FadeOut(milliseconds fade_time, int channel) const {
	auto time_int{ std::chrono::duration_cast<duration<int, std::milli>>(fade_time) };
	Mix_FadeOutChannel(channel, time_int.count());
}

void SoundManager::SetVolume(int channel, int volume) {
	PTGN_ASSERT(volume >= 0 && volume <= max_volume, "Cannot set sound channel volume outside of valid range");
	Mix_Volume(channel, volume);
}

int SoundManager::GetVolume(int channel) const {
	return Mix_Volume(channel, -1);
}

bool SoundManager::IsPlaying(int channel) const {
	return static_cast<bool>(Mix_Playing(channel));
}

bool SoundManager::IsPaused(int channel) const {
	return static_cast<bool>(Mix_Paused(channel));
}

bool SoundManager::IsFading(int channel) const {
	switch (Mix_FadingChannel(channel)) {
		case MIX_NO_FADING:	 return false;
		case MIX_FADING_OUT: return true;
		case MIX_FADING_IN:	 return true;
		default:			 return false;
	}
}

void SoundManager::Reset() {
	Stop(-1);
	MapManager::Reset();
}

} // namespace impl

} // namespace ptgn