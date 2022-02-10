#include "AnimationManager.h"

#include <cassert> // assert

#include "debugging/Debug.h"
#include "math/Hash.h"

namespace ptgn {

namespace impl {

DefaultAnimationManager::~DefaultAnimationManager() {
	// for (auto& [key, animation] : animation_map_) {
		// SDL_DestroyAnimation(animation.get());
	// }
}

void DefaultAnimationManager::LoadAnimation(const char* animation_key, const char* animation_path) {
	assert(animation_path != "" && "Cannot load empty animation path into default animation manager");
	assert(debug::FileExists(animation_path) && "Cannot load animation with non-existent file path into default animation manager");
	const auto key{ math::Hash(animation_key) };
	auto it{ animation_map_.find(key) };
	if (it == std::end(animation_map_)) {
		//auto temp_surface{ IMG_Load( animation_path ) };
		// if (temp_surface != nullptr) {
		// 	//auto& sdl_renderer{ GetSDLRenderer() };
		// 	//auto animation{ SDL_CreateAnimationFromSurface(sdl_renderer.renderer_, temp_surface) };
		// 	//auto shared_animation{ std::shared_ptr<SDL_Animation>(animation, SDL_DestroyAnimation) };
		// 	//animation_map_.emplace(key, shared_animation);
		// 	//SDL_FreeSurface(temp_surface);
		// } else {
		// 	debug::PrintLine("Failed to load animation into sdl animation manager: ", SDL_GetError());
		// }
	} else {
		debug::PrintLine("Warning: Cannot load animation key which already exists in the default animation manager");
	}
}

void DefaultAnimationManager::UnloadAnimation(const char* animation_key) {
	const auto key{ math::Hash(animation_key) }; 
	animation_map_.erase(key);
}

DefaultAnimationManager& GetDefaultAnimationManager() {
	static DefaultAnimationManager default_animation_manager;
	return default_animation_manager;
}

} // namespace impl

namespace services {

interfaces::AnimationManager& GetAnimationManager() {
	return impl::GetDefaultAnimationManager();
}

} // namespace services

} // namespace ptgn