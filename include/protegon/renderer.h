#pragma once

#include "color.h"

namespace ptgn {

enum class BlendMode {
	// Source: https://wiki.libsdl.org/SDL2/SDL_BlendMode

	NONE = 0x00000000, /*       no blending: dstRGBA = srcRGBA */
	BLEND = 0x00000001, /*    alpha blending: dstRGB = (srcRGB * srcA) + (dstRGB * (1 - srcA))
												 dstA = srcA + (dstA * (1-srcA)) */
	ADDITIVE = 0x00000002, /* additive blending: dstRGB = (srcRGB * srcA) + dstRGB
												 dstA = dstA */
	MODULATE = 0x00000004, /*    color modulate: dstRGB = srcRGB * dstRGB
												 dstA = dstA */
	MULTIPLY = 0x00000008, /*    color multiply: dstRGB = (srcRGB * dstRGB) + (dstRGB * (1 - srcA))
												 dstA = dstA */
	INVALID = 0x7FFFFFFF
};

namespace renderer {

void SetDrawColor(const Color& color);

} // namespace renderer

} // namespace ptgn
