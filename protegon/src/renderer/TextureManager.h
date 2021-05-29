#pragma once

#include <unordered_map> // std::unordered_map
#include <cstdint> // std::uint32_t
#include <cstdlib> // std::size_t

#include "renderer/Texture.h"
#include "utils/Singleton.h"

namespace engine {

class TextureManager : public Singleton<TextureManager> {
public:
	/*
	* Load texture of given path into the TextureManager.
	* @param texture_key Unique identifier associated with the loaded texture.
	* @param texture_path File path to load texture from (must end in .png or .jpg).
	*/
	static void Load(const char* texture_key, 
					 const char* texture_path);

	// Remove texture from TextureManager.
	static void Unload(const char* texture_key);

	// Return the location of a 4 byte integer value containg the RGBA32 color of the pixel on an SDL_Surface or SDL_Texture.
	static std::uint32_t& GetTexturePixel(void* pixels,
										  const int pitch,
										  const V2_int& position,
										  PixelFormat format);

	/*
	* @param texture_key Unique identifying key associated with the texture.
	* @return Reference to a const texture object with the given key.
	*/
	static const Texture& GetTexture(const char* texture_key);
private:
	friend class Engine;
	friend class Renderer;
	friend class Singleton<TextureManager>;

	TextureManager() = default;

	/*
	* Destroys all textures in the manager and clears the texture map.
	*/
	static void Destroy();
	
	// Texture storage container.
	std::unordered_map<std::size_t, Texture> texture_map_;
};

} // namespace engine