#include "protegon/audio.h"

#include <chrono>
#include <filesystem>
#include <ratio>
#include <string>

#include "SDL_mixer.h"
#include "core/sdl_instance.h"
#include "protegon/file.h"
#include "protegon/game.h"
#include "utility/debug.h"
#include "utility/handle.h"
#include "utility/time.h"

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

void Music::FadeIn(int loops, milliseconds time) {
	const auto time_int = std::chrono::duration_cast<duration<int, milliseconds::period>>(time);
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

void Sound::FadeIn(int channel, int loops, milliseconds time) {
	const auto time_int = std::chrono::duration_cast<duration<int, std::milli>>(time);
	Mix_FadeInChannel(channel, &Get(), loops, time_int.count());
}

void Sound::SetVolume(int volume) {
	Mix_VolumeChunk(&Get(), volume);
}

int Sound::GetVolume() {
	return Mix_VolumeChunk(&Get(), -1);
}

namespace impl {

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

void SoundManager::Stop(int channel) const {
	Mix_HaltChannel(channel);
}

void SoundManager::Resume(int channel) const {
	Mix_Resume(channel);
}

void SoundManager::FadeOut(int channel, milliseconds time) const {
	const auto time_int = std::chrono::duration_cast<duration<int, std::milli>>(time);
	Mix_FadeOutChannel(channel, time_int.count());
}

bool SoundManager::IsPlaying(int channel) const {
	return Mix_Playing(channel);
}

} // namespace impl

} // namespace ptgn