#pragma once

#include <unordered_map> // std::unordered_map
#include <cstdlib> // std::size_t

#include "renderer/Color.h"
#include "renderer/Texture.h"
#include "utils/Vector2.h"

namespace engine {

class FontManager {
public:
	static void Load(const char* text, const Color& color, const int size, const char* font_path);
	static void Draw(const char* text, V2_int position, V2_int size);
	static void Clean();
private:
	static void RemoveFont(const char* font_key);
	static Texture GetFont(const char* font_key);
	static std::unordered_map<std::size_t, Texture> font_map_;
};

} // namespace engine