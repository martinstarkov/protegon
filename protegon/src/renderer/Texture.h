#pragma once

#include "math/Vector2.h"
#include "renderer/Color.h"
#include "renderer/PixelFormat.h"

struct SDL_Texture;

namespace ptgn {

class Text;
class ScreenRenderer;
class TextureManager;
class Surface;

enum class TextureAccess : int {
	// Changes rarely, not lockable.
	STATIC = 0, // SDL_TEXTUREACCESS_STATIC = 0
	
	// Changes frequently, lockable.
	STREAMING = 1, // SDL_TEXTUREACCESS_STREAMING = 1

	// Can be used as a render target.
	TARGET = 2 // SDL_TEXTUREACCESS_TARGET = 2
};

// Make sure to free memory used by internal pointer using Destroy().
class Texture {
public:
	Texture() = default;
	~Texture() = default;
	Texture(const Texture& copy) = default;
	Texture(Texture&& move) = default;
	Texture& operator=(const Texture& copy) = default;
	Texture& operator=(Texture&& move) = default;

	// Locks texture to enable access to it.
	void Lock(void** out_pixels,
			  int* out_pitch,
			  V2_int lock_position = {}, 
			  V2_int lock_size = {});
	
	// Unlocks texture after access.
	void Unlock();
	
	// Sets all texture pixels to a specific color.
	void SetColor(const Color& color, PixelFormat format);

	V2_int GetSize() const;

	TextureAccess GetTextureAccess() const;

	std::uint32_t GetPixelFormat() const;

	PixelFormat AllocatePixelFormat(std::uint32_t format) const;

	void FreePixelFormat(PixelFormat format) const;

	int SlowGetBytesPerPixel() const;

	// Returns the pixel data at a given position on a texture.
	const std::uint32_t& GetPixel(void* pixels,
								  int pitch,
								  const V2_int& position,
								  int bytes_per_pixel) const;

	// Returns a reference to the pixel data at a given position on a texture.
	std::uint32_t& GetPixel(void* pixels,
							int pitch,
							const V2_int& position,
							int bytes_per_pixel);

	// Frees memory used by internal texture pointer.
	void Destroy();

	/*
	* @return True if texture is not nullptr, false otherwise.
	*/
	bool IsValid() const;
private:
	friend class TextureManager;
	friend class Text;
	friend class ScreenRenderer;

	// Creates texture with a given size and pixel format.
	Texture(const ScreenRenderer& renderer, 
			const V2_int& size, 
			std::uint32_t format, 
			TextureAccess texture_access = TextureAccess::STREAMING);

	// Creates texture from surface.
	Texture(const ScreenRenderer& renderer, const Surface& surface);

	SDL_Texture* operator=(SDL_Texture* texture);
	operator SDL_Texture* () const;
	SDL_Texture* operator&() const;

	Texture(SDL_Texture* texture);

	SDL_Texture* texture_{ nullptr };
};

} // namespace ptgn