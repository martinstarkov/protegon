#pragma once

#include <unordered_map> // std::unordered_map
#include <cstdlib> // std::size_t

#include "renderer/text/Font.h"
#include "utils/Singleton.h"

namespace ptgn {

class FontManager : public Singleton<FontManager> {
public:
	/*
	* Load font of given size into the FontManager.
	* When loading fonts remember to include size in the name for uniqueness.
	* @param name Unique identifier associated with the loaded font (should include size).
	* @param file File path to load True Type font from (must end in .ttf).
	* @param ptsize Point size (based on 72 DPI). This basically translates to pixel height.
	*/
	static void Load(const char* name, const char* file, std::uint32_t ptsize);
	
	// Remove font from FontManager.
	static void Unload(const char* name);
	
private:
	friend class Engine;
	friend class Text;
	friend class Singleton<FontManager>;

	/*
	* Destroys all fonts and clears internal font storage.
	*/
	static void Destroy();

	/*
	* @return True if FontManager contains the given font.
	*/
	static bool HasFont(std::size_t font_key);

	/*
	* @return Font object associated with the given font_key.
	*/
	static Font GetFont(std::size_t font_key);
	
	FontManager() = default;

	// Font object storage.
	std::unordered_map<std::size_t, Font> font_map_;
};

} // namespace ptgn