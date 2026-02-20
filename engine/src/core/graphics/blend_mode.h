#pragma once

#include <ostream>

#include "core/log.h"
#include "serialization/json/enum.h"

namespace ptgn {

/// @brief Defines how a source pixel (src) is composited onto a destination pixel (dst).
enum class BlendMode {
	/// No blending.
	/// dstRGB = srcRGB
	/// dstA   = srcA
	ReplaceRGBA,

	/// Standard alpha blending (non-premultiplied).
	/// dstRGB = srcRGB * srcA + dstRGB * (1 - srcA)
	/// dstA   = srcA + dstA * (1 - srcA)
	Blend,

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

inline std::ostream& operator<<(std::ostream& os, BlendMode blend_mode) {
	switch (blend_mode) {
		using enum ptgn::BlendMode;
		case Blend:						 os << "Blend"; break;
		case PremultipliedBlend:		 os << "PremultipliedBlend"; break;
		case ReplaceRGBA:				 os << "ReplaceRGBA"; break;
		case ReplaceRGB:				 os << "ReplaceRGB"; break;
		case ReplaceAlpha:				 os << "ReplaceAlpha"; break;
		case AddRGB:					 os << "AddRGB"; break;
		case AddRGBA:					 os << "AddRGBA"; break;
		case AddAlpha:					 os << "AddAlpha"; break;
		case PremultipliedAddRGB:		 os << "PremultipliedAddRGB"; break;
		case PremultipliedAddRGBA:		 os << "PremultipliedAddRGBA"; break;
		case MultiplyRGB:				 os << "MultiplyRGB"; break;
		case MultiplyRGBA:				 os << "MultiplyRGBA"; break;
		case MultiplyAlpha:				 os << "MultiplyAlpha"; break;
		case MultiplyRGBWithAlphaBlend:	 os << "MultiplyRGBWithAlphaBlend"; break;
		case MultiplyRGBAWithAlphaBlend: os << "MultiplyRGBAWithAlphaBlend"; break;
		default:						 PTGN_ERROR("Failed to identify blend mode");
	}
	return os;
}

PTGN_SERIALIZE_ENUM(
	BlendMode, { { BlendMode::Blend, "blend" },
				 { BlendMode::PremultipliedBlend, "premultiplied_blend" },
				 { BlendMode::ReplaceRGBA, "replace_rgba" },
				 { BlendMode::ReplaceRGB, "replace_rgb" },
				 { BlendMode::ReplaceAlpha, "replace_alpha" },
				 { BlendMode::AddRGB, "add_rgb" },
				 { BlendMode::AddRGBA, "add_rgba" },
				 { BlendMode::AddAlpha, "add_alpha" },
				 { BlendMode::PremultipliedAddRGB, "premultiplied_add_rgb" },
				 { BlendMode::PremultipliedAddRGBA, "premultiplied_add_rgba" },
				 { BlendMode::MultiplyRGB, "multiply_rgb" },
				 { BlendMode::MultiplyRGBA, "multiply_rgba" },
				 { BlendMode::MultiplyAlpha, "multiply_alpha" },
				 { BlendMode::MultiplyRGBWithAlphaBlend, "multiply_rgb_with_alpha_blend" },
				 { BlendMode::MultiplyRGBAWithAlphaBlend, "multiply_rgba_with_alpha_blend" } }
);

} // namespace ptgn