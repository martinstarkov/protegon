#include "audio/audio.h"

#include <chrono>
#include <filesystem>
#include <memory>
#include <ratio>
#include <string>

#include "debug/runtime/assert.h"
#include "core/ecs/components/generic.h"
#include "core/app/game.h"
#include "core/app/sdl_instance.h"
#include "core/utils/time.h"
#include "core/resource/resource_manager.h"
#include "SDL_mixer.h"
#include "core/utils/file.h"

namespace ptgn::impl {

void Mix_MusicDeleter::operator()(Mix_Music* music) const {
	if (game.sdl_instance_->SDLMixerIsInitialized()) {
		Mix_FreeMusic(music);
	}
}

void Mix_ChunkDeleter::operator()(Mix_Chunk* sound) const {
	if (game.sdl_instance_->SDLMixerIsInitialized()) {
		Mix_FreeChunk(sound);
	}
}

Music MusicManager::LoadFromFile(const path& filepath) {
	PTGN_ASSERT(
		FileExists(filepath), "Cannot create music from a nonexistent filepath: ", filepath.string()
	);
	Music ptr{ Mix_LoadMUS(filepath.string().c_str()) };
	PTGN_ASSERT(ptr != nullptr, Mix_GetError());
	return ptr;
}

void MusicManager::Play(const ResourceHandle& key, int loops) const {
	Mix_PlayMusic(Get(key).get(), loops);
}

void MusicManager::FadeIn(const ResourceHandle& key, milliseconds fade_time, int loops) const {
	auto time_int{ to_duration<duration<int, milliseconds::period>>(fade_time) };
	Mix_FadeInMusic(Get(key).get(), loops, time_int.count());
}

void MusicManager::Stop() const {
	Mix_HaltMusic();
}

void MusicManager::FadeOut(milliseconds time) const {
	auto time_int{ to_duration<duration<int, std::milli>>(time) };
	Mix_FadeOutMusic(time_int.count());
}

void MusicManager::Pause() const {
	Mix_PauseMusic();
}

void MusicManager::Resume() const {
	Mix_ResumeMusic();
}

void MusicManager::ToggleVolume(int new_volume) const {
	if (GetVolume() != 0) {
		SetVolume(0);
	} else {
		SetVolume(new_volume);
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
	PTGN_ASSERT(
		volume >= 0 && volume <= max_volume, "Cannot set music volume outside of valid range"
	);
	Mix_VolumeMusic(volume);
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

Sound SoundManager::LoadFromFile(const path& filepath) {
	PTGN_ASSERT(
		FileExists(filepath),
		"Cannot create sound from a nonexistent sound path: ", filepath.string()
	);
	Sound ptr{ Mix_LoadWAV(filepath.string().c_str()) };
	PTGN_ASSERT(ptr != nullptr, Mix_GetError());
	return ptr;
}

void SoundManager::Play(const ResourceHandle& key, int channel, int loops) const {
	PTGN_ASSERT(Has(key), "Cannot play sound which has not been loaded in the music manager");
	Mix_PlayChannel(channel, Get(key).get(), loops);
}

void SoundManager::FadeIn(const ResourceHandle& key, milliseconds fade_time, int channel, int loops)
	const {
	PTGN_ASSERT(Has(key), "Cannot fade in sound which has not been loaded in the music manager");
	auto time_int{ to_duration<duration<int, std::milli>>(fade_time) };
	Mix_FadeInChannel(channel, Get(key).get(), loops, time_int.count());
}

void SoundManager::SetVolume(const ResourceHandle& key, int volume) const {
	PTGN_ASSERT(
		Has(key), "Cannot set volume of sound which has not been loaded in the music manager"
	);
	PTGN_ASSERT(
		volume >= 0 && volume <= max_volume, "Cannot set sound volume outside of valid range"
	);
	Mix_VolumeChunk(Get(key).get(), volume);
}

void SoundManager::SetVolume(int channel, int volume) const {
	PTGN_ASSERT(
		volume >= 0 && volume <= max_volume,
		"Cannot set sound channel volume outside of valid range"
	);
	Mix_Volume(channel, volume);
}

int SoundManager::GetVolume(const ResourceHandle& key) const {
	PTGN_ASSERT(
		Has(key), "Cannot get volume of sound which has not been loaded in the music manager"
	);
	return Mix_VolumeChunk(Get(key).get(), -1);
}

void SoundManager::ToggleVolume(const ResourceHandle& key, int new_volume) const {
	PTGN_ASSERT(
		Has(key), "Cannot toggle volume of sound which has not been loaded in the music manager"
	);
	if (GetVolume(key) != 0) {
		SetVolume(key, 0);
	} else {
		SetVolume(key, new_volume);
	}
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
	auto time_int{ to_duration<duration<int, std::milli>>(fade_time) };
	Mix_FadeOutChannel(channel, time_int.count());
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

} // namespace ptgn::impl