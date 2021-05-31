#include "FontManager.h"

#include "debugging/Debug.h"
#include "math/Hasher.h"

namespace ptgn {

void FontManager::Load(const char* name, const char* file, std::uint32_t ptsize) {
	assert(name != "" && "Cannot load font with invalid key");
	assert(FileExists(file) && "Cannot load font with non-existent file path");
	assert(file != "" && "Cannot load font from empty file path");
	auto& instance{ GetInstance() };
	auto key{ Hasher::HashCString(name) };
	auto it{ instance.font_map_.find(key) };
	if (it == std::end(instance.font_map_)) { // Only load font if it doesn't already exist in font manager.
		Font font{ file, ptsize };
		instance.font_map_.emplace(key, font);
	} else {
		PrintLine("Warning: Cannot load font key which already exists in the FontManager");
	}
}

void FontManager::Unload(const char* name) {
	auto& instance{ GetInstance() };
	auto key{ Hasher::HashCString(name) };
	instance.font_map_.erase(key);
}

bool FontManager::HasFont(std::size_t font_key) {
	auto& instance{ GetInstance() };
	auto it{ instance.font_map_.find(font_key) };
	return it != std::end(instance.font_map_);
}

Font FontManager::GetFont(std::size_t font_key) {
	auto& instance{ GetInstance() };
	auto it{ instance.font_map_.find(font_key) };
	assert(it != std::end(instance.font_map_) && "Could not find font in font manager");
	return it->second;
}

void FontManager::Destroy() {
	auto& instance{ GetInstance() };
	for (auto& entry : instance.font_map_) {
		entry.second.Destroy();
	}
	instance.font_map_.clear();
}

} // namespace ptgn