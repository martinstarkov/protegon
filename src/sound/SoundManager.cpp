#include "SoundManager.h"

#include <cassert> // assert

#include "debugging/Debug.h"
#include "math/Math.h"
#include "renderer/Renderer.h"
#include "core/SDLManager.h"

namespace ptgn {

namespace impl {

SDLSoundManager::SDLSoundManager() {
	GetSDLManager();
}

SDLSoundManager::~SDLSoundManager() {
	for (auto& [key, sound] : sound_map_) {
		//SDL_DestroySound(sound.get());
	}
}

void SDLSoundManager::LoadSound(const char* sound_key, const char* sound_path) {
	assert(sound_path != "" && "Cannot load empty sound path into sdl sound manager");
	assert(debug::FileExists(sound_path) && "Cannot load sound with non-existent file path into sdl sound manager");
	const auto key{ math::Hash(sound_key) };
	auto it{ sound_map_.find(key) };
	if (it == std::end(sound_map_)) {
		// auto temp_surface{ IMG_Load( sound_path ) };
		// if (temp_surface != nullptr) {
		// 	auto& sdl_renderer{ GetSDLRenderer() };
		// 	auto sound{ SDL_CreateSoundFromSurface(sdl_renderer.renderer_, temp_surface) };
		// 	auto shared_sound{ std::shared_ptr<SDL_Sound>(sound, SDL_DestroySound) };
		// 	sound_map_.emplace(key, shared_sound);
		// 	SDL_FreeSurface(temp_surface);
		// } else {
		// 	debug::PrintLine("Failed to load sound into sdl sound manager: ", SDL_GetError());
		// }
	} else {
		debug::PrintLine("Warning: Cannot load sound key which already exists in the sdl sound manager");
	}
}

void SDLSoundManager::UnloadSound(const char* sound_key) {
	const auto key{ math::Hash(sound_key) }; 
	sound_map_.erase(key);
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