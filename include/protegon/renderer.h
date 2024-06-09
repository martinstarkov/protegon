#pragma once

#include "color.h"

namespace ptgn {

class Texture;

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
// Reset renderer draw color to transparent.
void ResetDrawColor();
void SetBlendMode(BlendMode mode);
void SetDrawMode(const Color& color, BlendMode mode);
// Clear screen.
void Clear();
// Push drawn objects to screen.
void Present();
void SetTarget(const Texture& texture);
// Reset renderer target to window.
void ResetTarget();

} // namespace renderer

} // namespace ptgn
