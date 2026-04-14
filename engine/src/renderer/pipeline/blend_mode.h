#pragma once

#include "serialization/serialize.h"

namespace ptgn {

/// @brief Defines how a source pixel (src) is composited onto a destination pixel (dst).
enum class BlendMode {
	/// Standard alpha blending (non-premultiplied).
	/// dstRGB = srcRGB * srcA + dstRGB * (1 - srcA)
	/// dstA   = srcA + dstA * (1 - srcA)
	Blend,

	/// No blending.
	/// dstRGB = srcRGB
	/// dstA   = srcA
	ReplaceRGBA,

	/// Alpha blending for premultiplied input.
	/// dstRGB = srcRGB + dstRGB * (1 - srcA)
	/// dstA   = srcA + dstA * (1 - srcA)
	PremultipliedBlend,

	/// Replace RGB only, preserve destination alpha.
	/// dstRGB = srcRGB
	/// dstA   = dstA
	ReplaceRGB,

	/// Replace alpha only, preserve destination color.
	/// dstRGB = dstRGB
	/// dstA   = srcA
	ReplaceAlpha,

	/// Additive RGB (scaled by source alpha), preserve destination alpha.
	/// dstRGB = srcRGB * srcA + dstRGB
	/// dstA   = dstA
	AddRGB,

	/// Additive RGB (scaled by source alpha) and additive alpha.
	/// dstRGB = srcRGB * srcA + dstRGB
	/// dstA   = srcA + dstA
	AddRGBA,

	/// Additive alpha only.
	/// dstRGB = dstRGB
	/// dstA   = srcA + dstA
	AddAlpha,

	/// Premultiplied additive RGB, preserve destination alpha.
	/// dstRGB = srcRGB + dstRGB
	/// dstA   = dstA
	PremultipliedAddRGB,

	/// Premultiplied additive RGB and additive alpha.
	/// dstRGB = srcRGB + dstRGB
	/// dstA   = srcA + dstA
	PremultipliedAddRGBA,

	/// Color multiply, preserve destination alpha.
	/// dstRGB = srcRGB * dstRGB
	/// dstA   = dstA
	MultiplyRGB,

	/// Color and alpha multiply.
	/// dstRGB = srcRGB * dstRGB
	/// dstA   = srcA * dstA
	MultiplyRGBA,

	/// Alpha multiply only.
	/// dstRGB = dstRGB
	/// dstA   = srcA * dstA
	MultiplyAlpha,

	/// Color multiply with alpha blending fallback.
	/// dstRGB = srcRGB * dstRGB + dstRGB * (1 - srcA)
	/// dstA   = dstA
	MultiplyRGBWithAlphaBlend,

	/// Color and alpha multiply with alpha blending semantics.
	/// dstRGB = srcRGB * dstRGB + dstRGB * (1 - srcA)
	/// dstA   = srcA * dstA
	MultiplyRGBAWithAlphaBlend
};
PTGN_SERIALIZE_ENUM(BlendMode);

} // namespace ptgn