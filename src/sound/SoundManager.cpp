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

void SDLSoundManager::LoadSound(const std::size_t sound_key, const char* sound_path) {
	assert(sound_path != "" && "Cannot load empty sound path into sdl sound manager");
	assert(debug::FileExists(sound_path) && "Cannot load sound with non-existent file path into sdl sound manager");
	auto temp_surface{ IMG_Load(sound_path) };
	//if (temp_surface != nullptr) {
		//auto texture{ CreateTextureFromSurface(temp_surface) };
		//SetTexture(texture_key, texture);
	//} else {
	//	debug::PrintLine("Failed to load texture into sdl texture manager: ", SDL_GetError());
	//}
	//SDL_FreeSurface(temp_surface);
}

void SDLSoundManager::UnloadSound(const std::size_t sound_key) {
	sound_map_.erase(sound_key);
}
void SDLSoundManager::UnloadMusic(const std::size_t music_key) {
	sound_map_.erase(sound_key);
}

bool SDLSoundManager::HasSound(const std::size_t sound_key) const {
	auto it{ sound_map_.find(sound_key) };
	return it != std::end(sound_map_) && it->second != nullptr;
}

void SDLSoundManager::SetSound(const std::size_t sound_key, Sound* sound) {
	auto it{ sound_map_.find(sound_key) };
	if (it != std::end(sound_map_)) {
		if (it->second.get() != sound) {
			it->second.reset(sound);
		}
	} else {
		sound_map_.emplace(sound_key, sound);
	}
}

Sound* SDLSoundManager::GetSound(const std::size_t sound_key) {
	auto it{ sound_map_.find(sound_key) };
	if (it != std::end(sound_map_)) {
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