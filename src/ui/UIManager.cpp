#include "UIManager.h"

#include <cassert> // assert

#include "debugging/Debug.h"
#include "math/Math.h"
#include "renderer/Renderer.h"

namespace ptgn {

namespace impl {

DefaultUIManager::~DefaultUIManager() {
	for (auto& [key, ui] : ui_map_) {
		//Default_DestroyUI(ui.get());
	}
}

void DefaultUIManager::LoadUI(const char* ui_key, const char* ui_path) {
	assert(ui_path != "" && "Cannot load empty ui path into default ui manager");
	assert(debug::FileExists(ui_path) && "Cannot load ui with non-existent file path into default ui manager");
	const auto key{ math::Hash(ui_key) };
	auto it{ ui_map_.find(key) };
	if (it == std::end(ui_map_)) {
		// auto temp_surface{ IMG_Load( ui_path ) };
		// if (temp_surface != nullptr) {
		// 	auto& sdl_renderer{ GetDefaultRenderer() };
		// 	auto ui{ Default_CreateUIFromSurface(sdl_renderer.renderer_, temp_surface) };
		// 	auto shared_ui{ std::shared_ptr<Default_UI>(ui, Default_DestroyUI) };
		// 	ui_map_.emplace(key, shared_ui);
		// 	Default_FreeSurface(temp_surface);
		// } else {
		// 	debug::PrintLine("Failed to load ui into sdl ui manager: ", Default_GetError());
		// }
	} else {
		debug::PrintLine("Warning: Cannot load ui key which already exists in the default UI manager");
	}
}

void DefaultUIManager::UnloadUI(const char* ui_key) {
	const auto key{ math::Hash(ui_key) }; 
	ui_map_.erase(key);
}

// std::shared_ptr<Default_UI> DefaultUIManager::GetUI(const char* ui_key) {
// 	const auto key{ math::Hash(ui_key) };
// 	auto it{ ui_map_.find(key) };
// 	if (it != std::end(ui_map_)) {
// 		return it->second;
// 	}
// 	return nullptr;
// }

DefaultUIManager& GetDefaultUIManager() {
	static DefaultUIManager default_ui_manager;
	return default_ui_manager;
}

} // namespace impl

namespace services {

interfaces::UIManager& GetUIManager() {
	return impl::GetDefaultUIManager();
}

} // namespace services

} // namespace ptgn