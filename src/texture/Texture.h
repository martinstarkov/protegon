#pragma once

#include "math/Vector2.h"
#include "texture/Flip.h"
#include "renderer/Renderer.h"

struct SDL_Texture;
struct SDL_Surface;

namespace ptgn {

namespace internal {

struct Texture {
public:
	Texture() = default;
	Texture(const Renderer& renderer);
	Texture(const Renderer& renderer, const char* texture_path);
	Texture(const Renderer& renderer, SDL_Surface* surface);
	~Texture();
	void Reset(SDL_Surface* surface);
	// Draws the texture to the screen.
	void Draw(const V2_int& texture_position,
		      const V2_int& texture_size,
		      const V2_int& source_position,
		      const V2_int& source_size) const;
	// Draws the texture to the screen. Allows for rotation and texture flipping.
	// Set center_of_rotation to nullptr if center of rotation is desired to be the center of the texture.
	void Draw(const V2_int& texture_position,
		      const V2_int& texture_size,
		      const V2_int& source_position,
		      const V2_int& source_size,
		      const V2_int* center_of_rotation,
		      const double angle,
		      Flip flip = Flip::NONE) const;
	operator SDL_Texture*() const;
	const Renderer& GetRenderer() const;
private:
	void Set(SDL_Surface* surface);
	const Renderer* renderer_{ nullptr };
	SDL_Texture* texture_{ nullptr };
};

} // namespace internal

} // namespace ptgn