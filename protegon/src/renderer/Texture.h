#pragma once

#include "math/Vector2.h"

#include "renderer/Color.h"
#include "renderer/Surface.h"

struct SDL_Texture;

namespace engine {

class Renderer;

enum class TextureAccess : int {
	STATIC = 0, // SDL_TEXTUREACCESS_STATIC = 0
	STREAMING = 1, // SDL_TEXTUREACCESS_STREAMING = 1
	TARGET = 2 // SDL_TEXTUREACCESS_TARGET = 2
};

enum class PixelFormat : std::uint32_t {
	ARGB8888 = 372645892, // SDL_PIXELFORMAT_ARGB8888 = 372645892
	RGBA8888 = 373694468 // SDL_PIXELFORMAT_RGBA8888 = 373694468
};

class Texture {
public:
	Texture() = default;
	// Note that textures have internal heap allocated
	// memory which must be freed by calling Destroy().
	~Texture();
	Texture(SDL_Texture* texture);
	Texture(const Renderer& renderer, const V2_int& size, PixelFormat format, TextureAccess texture_access);
	Texture(const Renderer& renderer, const Surface& surface);
	SDL_Texture* operator=(SDL_Texture* texture);
	operator SDL_Texture* () const;
	SDL_Texture* operator&() const;
	bool IsValid() const;
	// Locks texture to enable writing to it.
	bool Lock(void** out_pixels, int* out_pitch, V2_int lock_position = {}, V2_int lock_size = {});
	void Unlock();
	void Destroy();
	// Sets all texture pixels to a specific color.
	void SetColor(const Color& color);
	// Sets all texture pixels to black.
	void Clear();
private:
	SDL_Texture* texture{ nullptr };
};

} // namespace engine