#pragma once

#include <memory> // std::shared_ptr

#include "polygon.h"
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

enum class DrawMode {
	NONE = 0x00000000,     /**< no blending
											  dstRGBA = srcRGBA */
	BLEND = 0x00000001,    /**< alpha blending
											  dstRGB = (srcRGB * srcA) + (dstRGB * (1-srcA))
											  dstA = srcA + (dstA * (1-srcA)) */
	ADD = 0x00000002,      /**< additive blending
											  dstRGB = (srcRGB * srcA) + dstRGB
											  dstA = dstA */
	MOD = 0x00000004,      /**< color modulate
											  dstRGB = srcRGB * dstRGB
											  dstA = dstA */
	MUL = 0x00000008,      /**< color multiply
											  dstRGB = (srcRGB * dstRGB) + (dstRGB * (1-srcA))
											  dstA = dstA */
	INVALID = 0x7FFFFFFF
};

class Texture {
public:
	Texture() = default;
	// TODO: Switch to fs::path
	Texture(const char* image_path);
	~Texture() = default;
	Texture(const Texture&) = default;
	Texture& operator=(const Texture&) = default;
	Texture(Texture&&) = default;
	Texture& operator=(Texture&&) = default;
	bool IsValid() const;
	// Rotation in degrees. Positive clockwise.
	void Draw(const Rectangle<float>& texture,
			  const Rectangle<int>& source = {},
			  float angle = 0.0f,
			  Flip flip = Flip::NONE,
			  V2_int* center_of_rotation = nullptr) const;
	V2_int GetSize() const;
	void SetAlpha(std::uint8_t alpha);
	void SetColor(const Color& color);
private:
	friend class Text;
	// Takes ownership of surface pointer.
	Texture(SDL_Surface* surface);
	std::shared_ptr<SDL_Texture> texture_{ nullptr };
};

} // namespace ptgn