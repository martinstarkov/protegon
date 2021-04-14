#include "FontManager.h"

#include <cassert> // assert

#include "math/Hasher.h"

namespace engine {

std::unordered_map<std::size_t, Font> FontManager::font_map_;

void FontManager::Load(const char* name, const char* file, std::uint32_t ptsize) {
	assert(name != "" && "Cannot load font with invalid key");
	// TODO: Add test for validity of file path.
	assert(file != "" && "Cannot load font from empty file path");
	auto key{ Hasher::HashCString(name) };
	auto it{ font_map_.find(key) };
	if (it == std::end(font_map_)) { // Only load font if it doesn't already exist in font manager.
		Font font{ file, ptsize };
		font_map_.emplace(key, font);
	}
}

void FontManager::Unload(const char* name) {
	auto key{ Hasher::HashCString(name) };
	font_map_.erase(key);
}

Font FontManager::GetFont(std::size_t font_key) {
	auto it{ font_map_.find(font_key) };
	assert(it != std::end(font_map_) && "Could not find font in font manager");
	return it->second;
}

void FontManager::Clear() {
	for (auto& entry : font_map_) {
		entry.second.Destroy();
	}
	font_map_.clear();
}

} // namespace engine