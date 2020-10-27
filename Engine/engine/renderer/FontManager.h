#pragma once

#include <string>
#include <unordered_map>

#include "renderer/Color.h"
#include "renderer/Texture.h"
#include "utils/Vector2.h"

namespace engine {

class FontManager {
public:
	static Texture Load(std::string& text, const Color& color, const int size, const char* path);
	static void Draw(const std::string& text, V2_int position, V2_int size);
	static void RemoveFont(const std::string& key);
	static void Clean();
private:
	static Texture GetFont(const std::string& key);
	static std::unordered_map<std::string, Texture> font_map_;
};

} // namespace engine