#pragma once

#include <unordered_map> // std::unordered_map
#include <cstdint> // std::uint32_t
#include <cstdlib> // std::size_t

#include "renderer/Texture.h"

namespace engine {

class TextureManager {
private:
public:
	static void Load(const char* texture_key,
					 const char* texture_path,
					 std::size_t display_index = 0);

	static Texture GetTexture(const char* texture_key);

	// Return the location of a 4 byte integer value containg the RGBA32 color of the pixel on an SDL_Surface or SDL_Texture.
	static std::uint32_t& GetTexturePixel(void* pixels, 
										  const int pitch, 
										  const V2_int& position);
	static void Clear();
	static void Remove(const char* texture_key);
private:
	static std::unordered_map<std::size_t, Texture> texture_map_;
};

} // namespace engine