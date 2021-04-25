#pragma once

#include <unordered_map> // std::unordered_map
#include <cstdlib> // std::size_t

#include "renderer/text/Font.h"

namespace engine {

class FontManager {
public:
	/*
	* Load font of given size into the font manager.
	* When loading fonts remember to include size in the name for uniqueness.
	* @param name - Unique identifier associated with the loaded font (should include size).
	* @param file - File path to load True Type font from (must end in .ttf).
	* @param ptsize - Point size (based on 72 DPI). This basically translates to pixel height.
	*/
	static void Load(const char* name, const char* file, std::uint32_t ptsize);
	// Remove font from font manager.
	static void Unload(const char* name);
	// Destroys all fonts and clears internal font storage.
	static void Clear();
private:
	friend class Text;

	static Font GetFont(std::size_t font_key);

	// Font object storage.
	static std::unordered_map<std::size_t, Font> font_map_;
};

} // namespace engine