#pragma once

#include <memory> // std::shared_ptr

#include "rectangle.h"
#include "vector2.h"
#include "color.h"

struct SDL_Texture;
struct SDL_Surface;

namespace ptgn {

enum class Flip
{
	NONE       = 0x00000000,
	HORIZONTAL = 0x00000001,
	VERTICAL   = 0x00000002
};

class Texture {
public:
	Texture() = default;
	Texture(const char* image_path);
	~Texture() = default;
	Texture(const Texture&) = default;
	Texture& operator=(const Texture&) = default;
	Texture(Texture&&) = default;
	Texture& operator=(Texture&&) = default;
	bool IsValid() const;
	// Rotation is clockwise.
	void Draw(Rectangle<float> texture,
			  const Rectangle<int>& source = {},
			  float angle = 0.0f,
			  Flip flip = Flip::NONE,
			  V2_int* center_of_rotation = nullptr) const;
	V2_int GetSize() const;
	void SetAlpha(std::uint8_t alpha);
private:
	friend class Text;
	// Takes ownership of surface pointer.
	Texture(SDL_Surface* surface);
	std::shared_ptr<SDL_Texture> texture_{ nullptr };
};

} // namespace ptgn