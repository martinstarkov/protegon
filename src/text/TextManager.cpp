#include "TextManager.h"

#include <cassert> // assert

#include "debugging/Debug.h"
#include "math/Math.h"
#include "renderer/Renderer.h"

namespace ptgn {

namespace impl {

DefaultTextManager::~DefaultTextManager() {
	for (auto& [key, text] : text_map_) {
		//SDL_DestroyText(text.get());
	}
}

void DefaultTextManager::LoadText(const char* text_key, const char* text_path) {
	assert(text_path != "" && "Cannot load empty text path into default text manager");
	assert(debug::FileExists(text_path) && "Cannot load text with non-existent file path into default text manager");
	const auto key{ math::Hash(text_key) };
	auto it{ text_map_.find(key) };
	if (it == std::end(text_map_)) {
		//auto temp_surface{ IMG_Load( text_path ) };
		// if (temp_surface != nullptr) {
		// 	auto& default_renderer{ GetSDLRenderer() };
		// 	auto text{ SDL_CreateTextFromSurface(default_renderer.renderer_, temp_surface) };
		// 	auto shared_text{ std::shared_ptr<SDL_Text>(text, SDL_DestroyText) };
		// 	text_map_.emplace(key, shared_text);
		// 	SDL_FreeSurface(temp_surface);
		// } else {
		// 	debug::PrintLine("Failed to load text into default text manager: ", SDL_GetError());
		// }
	} else {
		debug::PrintLine("Warning: Cannot load text key which already exists in the default text manager");
	}
}

void DefaultTextManager::UnloadText(const char* text_key) {
	const auto key{ math::Hash(text_key) }; 
	text_map_.erase(key);
}

// std::shared_ptr<SDL_Text> DefaultTextManager::GetText(const char* text_key) {
// 	const auto key{ math::Hash(text_key) };
// 	auto it{ text_map_.find(key) };
// 	if (it != std::end(text_map_)) {
// 		return it->second;
// 	}
// 	return nullptr;
// }

DefaultTextManager& GetDefaultTextManager() {
	static DefaultTextManager default_text_manager;
	return default_text_manager;
}

} // namespace impl

namespace services {

interfaces::TextManager& GetTextManager() {
	return impl::GetDefaultTextManager();
}

} // namespace services

} // namespace ptgn