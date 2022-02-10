#include "SoundManager.h"

#include <cassert> // assert

#include <SDL.h>
#include <SDL_mixer.h>

#include "debugging/Debug.h"
#include "math/Math.h"
#include "core/SDLManager.h"

namespace ptgn {

namespace impl {

void SDLSoundDeleter::operator()(Mix_Chunk* sound) {
	Mix_FreeChunk(sound);
}

void SDLMusicDeleter::operator()(Mix_Music* music) {
	Mix_FreeMusic(music);
}

SDLSoundManager::SDLSoundManager() {
	GetSDLManager();
}

void SDLSoundManager::PlaySound(const std::size_t key, int channel, int loops) const {
	auto sound = GetSound(key);
	assert(sound != nullptr && "Unable to play sound which has not been loaded into sdl sound manager");
	Mix_PlayChannel(channel, sound, loops);
}

void SDLSoundManager::PauseSound(int channel) const {
	Mix_Pause(channel);
}

void SDLSoundManager::ResumeSound(int channel) const {
	Mix_Resume(channel);
}

void SDLSoundManager::StopSound(int channel) const {
	Mix_HaltChannel(channel);
}

void SDLSoundManager::FadeOutSound(int channel, milliseconds time) const {
	Mix_FadeOutChannel(channel, time.count());
}

bool SDLSoundManager::IsSoundPlaying(int channel) const {
	return Mix_Playing(channel);
}

bool SDLSoundManager::IsSoundPaused(int channel) const {
	return Mix_Paused(channel);
}

bool SDLSoundManager::IsSoundFading(int channel) const {
	switch (Mix_FadingChannel(channel)) {
		case MIX_NO_FADING:
			return false;
		case MIX_FADING_OUT:
			return true;
		case MIX_FADING_IN:
			return true;
		default:
			return false;
	}
}

void SDLSoundManager::PlayMusic(const std::size_t key, int loops) const {
	auto music = GetMusic(key);
	assert(music != nullptr && "Unable to play music which has not been loaded into sdl sound manager");
	Mix_PlayMusic(music, loops);
}

void SDLSoundManager::StopMusic() const {
	Mix_HaltMusic();
}

void SDLSoundManager::FadeInSound(const std::size_t key, int channel, int loops, milliseconds time) const {
	auto sound = GetSound(key);
	assert(sound != nullptr && "Unable to fade in sound which has not been loaded into sdl sound manager");
	Mix_FadeInChannel(channel, sound, loops, time.count());
}

void SDLSoundManager::FadeInMusic(const std::size_t key, int loops, milliseconds time) const {
	auto music = GetMusic(key);
	assert(music != nullptr && "Unable to fade in music which has not been loaded into sdl sound manager");
	Mix_FadeInMusic(music, loops, time.count());
}

void SDLSoundManager::FadeOutMusic(milliseconds time) const {
	Mix_FadeOutMusic(time.count());
}

void SDLSoundManager::PauseMusic() const {
	Mix_PauseMusic();
}

void SDLSoundManager::ResumeMusic() const {
	Mix_ResumeMusic();
}

bool SDLSoundManager::IsMusicPlaying() const {
	return Mix_PlayingMusic();
}

bool SDLSoundManager::IsMusicPaused() const {
	return Mix_PausedMusic();
}
    
bool SDLSoundManager::IsMusicFading() const {
	switch (Mix_FadingMusic()) {
		case MIX_NO_FADING:
			return false;
		case MIX_FADING_OUT:
			return true;
		case MIX_FADING_IN:
			return true;
		default:
			return false;
	}
}

void SDLSoundManager::LoadSound(const std::size_t key, const char* path) {
	assert(path != "" && "Cannot load empty sound path into sdl sound manager");
	assert(debug::FileExists(path) && "Cannot load sound with non-existent file path into sdl sound manager");
	auto sound = Mix_LoadWAV(path);
    if (sound != NULL) {
		SetSound(key, sound);
	} else {
		debug::PrintLine("Failed to load sound into sdl sound manager: ", Mix_GetError());
    }
}

void SDLSoundManager::LoadMusic(const std::size_t key, const char* path) {
	assert(path != "" && "Cannot load empty music path into sdl sound manager");
	assert(debug::FileExists(path) && "Cannot load music with non-existent file path into sdl sound manager");
	auto music = Mix_LoadMUS(path);
    if (music != NULL) {
		SetMusic(key, music);
	} else {
		debug::PrintLine("Failed to load music into sdl sound manager: ", Mix_GetError());
    }
}

void SDLSoundManager::UnloadSound(const std::size_t key) {
	sound_map_.erase(key);
}

void SDLSoundManager::UnloadMusic(const std::size_t key) {
	music_map_.erase(key);
}

bool SDLSoundManager::HasSound(const std::size_t key) const {
	auto it{ sound_map_.find(key) };
	return it != std::end(sound_map_) && it->second != nullptr;
}

bool SDLSoundManager::HasMusic(const std::size_t key) const {
	auto it{ music_map_.find(key) };
	return it != std::end(music_map_) && it->second != nullptr;
}

void SDLSoundManager::SetSound(const std::size_t key, Mix_Chunk* sound) {
	auto it{ sound_map_.find(key) };
	if (it != std::end(sound_map_)) {
		if (it->second.get() != sound) {
			it->second.reset(sound);
		}
	} else {
		sound_map_.emplace(key, sound);
	}
}

void SDLSoundManager::SetMusic(const std::size_t key, Mix_Music* music) {
	auto it{ music_map_.find(key) };
	if (it != std::end(music_map_)) {
		if (it->second.get() != music) {
			it->second.reset(music);
		}
	} else {
		music_map_.emplace(key, music);
	}
}

Mix_Chunk* SDLSoundManager::GetSound(const std::size_t key) const {
	auto it{ sound_map_.find(key) };
	if (it != std::end(sound_map_)) {
		return it->second.get();
	}
	return nullptr;
}

Mix_Music* SDLSoundManager::GetMusic(const std::size_t key) const {
	auto it{ music_map_.find(key) };
	if (it != std::end(music_map_)) {
		return it->second.get();
	}
	return nullptr;
}

SDLSoundManager& GetSDLSoundManager() {
	static SDLSoundManager default_sound_manager;
	return default_sound_manager;
}

} // namespace impl

namespace services {

interfaces::SoundManager& GetSoundManager() {
	return impl::GetSDLSoundManager();
}

} // namespace services

} // namespace ptgn