#pragma once

#include "math/Vector2.h"

#include "renderer/Color.h"
#include "renderer/Surface.h"
#include "renderer/sprites/PixelFormat.h"

struct SDL_Texture;

namespace engine {

class Renderer;

enum class TextureAccess : int {
	// changes rarely, not lockable
	STATIC = 0, // SDL_TEXTUREACCESS_STATIC = 0
	// changes frequently, lockable
	STREAMING = 1, // SDL_TEXTUREACCESS_STREAMING = 1
	// can be used as a render target
	TARGET = 2 // SDL_TEXTUREACCESS_TARGET = 2
};

// Textures must be freed using Destroy().
class Texture {
public:
	Texture() = default;
	Texture(const Renderer& renderer, const V2_int& size, PixelFormat pixel_format = PixelFormat::RGBA8888, TextureAccess texture_access = TextureAccess::STREAMING);
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
	void SetColor(const Color& color, PixelFormat pixel_format = PixelFormat::RGBA8888);
	// Sets all texture pixels to black.
	void Clear(PixelFormat pixel_format = PixelFormat::RGBA8888);
private:

	Texture(SDL_Texture* texture);

	SDL_Texture* texture_{ nullptr };
};

} // namespace engine