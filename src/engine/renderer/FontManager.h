#pragma once

#include <string>
#include <unordered_map>

#include <engine/renderer/Color.h>
#include <engine/utils/Vector2.h>

namespace engine {

class FontManager {
public:
	static SDL_Texture& Load(std::string& text, const Color& color, const int size, const char* path);
	static void Draw(const std::string& text, V2_int position, V2_int size);
	static void RemoveFont(const std::string& key);
	static void Clean();
private:
	static SDL_Texture& GetFont(const std::string& key);
	static std::unordered_map<std::string, SDL_Texture*> font_map_;
};

} // namespace engine