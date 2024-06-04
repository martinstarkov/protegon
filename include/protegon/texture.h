#pragma once

#include "polygon.h"
#include "vector2.h"
#include "color.h"
#include "handle.h"
#include "file.h"

struct SDL_Texture;
struct SDL_Surface;

namespace ptgn {

class Texture : public Handle<SDL_Texture> {
public:
	enum class Flip {
		// Source: https://wiki.libsdl.org/SDL2/SDL_RendererFlip

		NONE       = 0x00000000,
		HORIZONTAL = 0x00000001,
		VERTICAL   = 0x00000002
	};

	enum class DrawMode {
		// Source: https://wiki.libsdl.org/SDL2/SDL_BlendMode

		NONE     = 0x00000000, /*       no blending: dstRGBA = srcRGBA */
		BLEND    = 0x00000001, /*    alpha blending: dstRGB = (srcRGB * srcA) + (dstRGB * (1 - srcA))
				  							         dstA = srcA + (dstA * (1-srcA)) */
		ADDITIVE = 0x00000002, /* additive blending: dstRGB = (srcRGB * srcA) + dstRGB
				  							         dstA = dstA */
		MODULATE = 0x00000004, /*    color modulate: dstRGB = srcRGB * dstRGB
				  					 		         dstA = dstA */
		MULTIPLY = 0x00000008, /*    color multiply: dstRGB = (srcRGB * dstRGB) + (dstRGB * (1 - srcA))
			    								     dstA = dstA */
		INVALID  = 0x7FFFFFFF
	};

	Texture() = default;
	Texture(const path& image_path);

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
	Texture(const std::shared_ptr<SDL_Surface>& surface);
};

} // namespace ptgn