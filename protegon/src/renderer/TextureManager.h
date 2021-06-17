#pragma once

#include <unordered_map> // std::unordered_map
#include <cstdint> // std::uint32_t
#include <cstdlib> // std::size_t

#include "renderer/Texture.h"
#include "utils/Singleton.h"

namespace ptgn {

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

	/*
	* @param texture_key Unique identifying key associated with the texture.
	* @return Reference to a const texture object with the given key.
	* If TextureManager does not contain the requested texture, assertion is called.
	*/
	static const Texture& GetTexture(const char* texture_key);
private:
	friend class Engine;
	friend class ScreenRenderer;
	friend class Singleton<TextureManager>;

	TextureManager() = default;

	/*
	* Destroys all textures in the manager and clears the texture map.
	*/
	static void Destroy();
	
	// Texture storage container.
	std::unordered_map<std::size_t, Texture> texture_map_;
};

} // namespace ptgn