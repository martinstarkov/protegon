#include "SceneManager.h"

#include <cassert> // assert

#include "debugging/Debug.h"
#include "math/Hash.h"
#include "renderer/Renderer.h"

namespace ptgn {

namespace impl {

DefaultSceneManager::~DefaultSceneManager() {
	for (auto& [key, scene] : scene_map_) {
		//SDL_DestroyScene(scene.get());
	}
}

void DefaultSceneManager::LoadScene(const char* scene_key, const char* scene_path) {
	assert(scene_path != "" && "Cannot load empty scene path into default scene manager");
	assert(debug::FileExists(scene_path) && "Cannot load scene with non-existent file path into default scene manager");
	const auto key{ math::Hash(scene_key) };
	auto it{ scene_map_.find(key) };
	if (it == std::end(scene_map_)) {
		// auto temp_surface{ IMG_Load( scene_path ) };
		// if (temp_surface != nullptr) {
		// 	auto& sdl_renderer{ GetSDLRenderer() };
		// 	auto scene{ SDL_CreateSceneFromSurface(sdl_renderer.renderer_, temp_surface) };
		// 	auto shared_scene{ std::shared_ptr<SDL_Scene>(scene, SDL_DestroyScene) };
		// 	scene_map_.emplace(key, shared_scene);
		// 	SDL_FreeSurface(temp_surface);
		// } else {
		// 	debug::PrintLine("Failed to load scene into sdl scene manager: ", SDL_GetError());
		// }
	} else {
		debug::PrintLine("Warning: Cannot load scene key which already exists in the default scene manager");
	}
}

void DefaultSceneManager::UnloadScene(const char* scene_key) {
	const auto key{ math::Hash(scene_key) }; 
	scene_map_.erase(key);
}

// std::shared_ptr<SDL_Scene> DefaultSceneManager::GetScene(const char* scene_key) {
// 	const auto key{ math::Hash(scene_key) };
// 	auto it{ scene_map_.find(key) };
// 	if (it != std::end(scene_map_)) {
// 		return it->second;
// 	}
// 	return nullptr;
// }

DefaultSceneManager& GetDefaultSceneManager() {
	static DefaultSceneManager default_scene_manager;
	return default_scene_manager;
}

} // namespace impl

namespace services {

interfaces::SceneManager& GetSceneManager() {
	return impl::GetDefaultSceneManager();
}

} // namespace services

} // namespace ptgn