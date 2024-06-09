#pragma once

#include "polygon.h"
#include "color.h"

namespace ptgn {

class Texture;

enum class BlendMode {
	// Source: https://wiki.libsdl.org/SDL2/SDL_BlendMode

	None = 0x00000000, /*       no blending: dstRGBA = srcRGBA */
	Blend = 0x00000001, /*    alpha blending: dstRGB = (srcRGB * srcA) + (dstRGB * (1 - srcA))
												 dstA = srcA + (dstA * (1-srcA)) */
	Add = 0x00000002, /* additive blending: dstRGB = (srcRGB * srcA) + dstRGB
												 dstA = dstA */
	Modulate = 0x00000004, /*    color modulate: dstRGB = srcRGB * dstRGB
												 dstA = dstA */
	Multiply = 0x00000008, /*    color multiply: dstRGB = (srcRGB * dstRGB) + (dstRGB * (1 - srcA))
												 dstA = dstA */
	Invalid = 0x7FFFFFFF
};

enum class Flip {
	// Source: https://wiki.libsdl.org/SDL2/SDL_RendererFlip

	None = 0x00000000,
	Horizontal = 0x00000001,
	Vertical = 0x00000002
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
void DrawTexture(
	const Texture& texture,
	const Rectangle<int>& destination_rect = {},
	const Rectangle<int>& source_rect = {},
	float angle = 0.0f,
	Flip flip = Flip::None,
	V2_int* center_of_rotation = nullptr
);

} // namespace renderer

} // namespace ptgn
