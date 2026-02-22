#pragma once

#include <iosfwd>
#include <ostream>

#include "debug/log.h"
#include "serialization/enum.h"

namespace ptgn {

enum class BlendMode {
	Blend,
	/**< Alpha blending:
		 dstRGB = srcRGB * srcA + dstRGB * (1 - srcA)
		 dstA   = srcA + dstA * (1 - srcA) */

	PremultipliedBlend,
	/**< Premultiplied alpha blending:
		 dstRGB = srcRGB + dstRGB * (1 - srcA)
		 dstA = srcA + dstA * (1 - srcA) */

	ReplaceRGBA,
	/**< Replace RGBA:
		 dstRGB = srcRGB
		 dstA   = srcA */

	ReplaceRGB,
	/**< Replace RGB:
		 dstRGB = srcRGB
		 dstA   = dstA */

	ReplaceAlpha,
	/**< Replace alpha:
		 dstRGB = dstRGB
		 dstA   = srcA */

	AddRGB,
	/**< Additive blending:
		 dstRGB = srcRGB * srcA + dstRGB
		 dstA   = dstA */

	AddRGBA,
	/**< Additive blending with alpha:
		 dstRGB = srcRGB * srcA + dstRGB
		 dstA   = srcA + dstA */

	AddAlpha,
	/**< Additive blending for only alpha:
		 dstRGB = dstRGB
		 dstA   = srcA + dstA */

	PremultipliedAddRGB,
	/**< Premultiplied additive blending:
		 dstRGB = srcRGB + dstRGB
		 dstA   = dstA */

	PremultipliedAddRGBA,
	/**< Premultiplied additive blending with alpha:
		 dstRGB = srcRGB + dstRGB
		 dstA   = srcA + dstA */

	MultiplyRGB,
	/**< Color multiply:
		 dstRGB = srcRGB * dstRGB
		 dstA   = dstA */

	MultiplyRGBA,
	/**< Color multiply with alpha:
		 dstRGB = srcRGB * dstRGB
		 dstA   = srcA * dstA */

	MultiplyAlpha,
	/**< Alpha multiply:
		 dstRGB = dstRGB
		 dstA   = srcA * dstA */

	MultiplyRGBWithAlphaBlend,
	/**< Color multiply:
		 dstRGB = srcRGB * dstRGB + dstRGB * (1 - srcA)
		 dstA   = dstA */

	MultiplyRGBAWithAlphaBlend
	/**< Color multiply:
		 dstRGB = srcRGB * dstRGB + dstRGB * (1 - srcA)
		 dstA   = srcA * dstA */
};

inline std::ostream& operator<<(std::ostream& os, BlendMode blend_mode) {
	switch (blend_mode) {
		case BlendMode::Blend:						os << "Blend"; break;
		case BlendMode::PremultipliedBlend:			os << "PremultipliedBlend"; break;
		case BlendMode::ReplaceRGBA:				os << "ReplaceRGBA"; break;
		case BlendMode::ReplaceRGB:					os << "ReplaceRGB"; break;
		case BlendMode::ReplaceAlpha:				os << "ReplaceAlpha"; break;
		case BlendMode::AddRGB:						os << "AddRGB"; break;
		case BlendMode::AddRGBA:					os << "AddRGBA"; break;
		case BlendMode::AddAlpha:					os << "AddAlpha"; break;
		case BlendMode::PremultipliedAddRGB:		os << "PremultipliedAddRGB"; break;
		case BlendMode::PremultipliedAddRGBA:		os << "PremultipliedAddRGBA"; break;
		case BlendMode::MultiplyRGB:				os << "MultiplyRGB"; break;
		case BlendMode::MultiplyRGBA:				os << "MultiplyRGBA"; break;
		case BlendMode::MultiplyAlpha:				os << "MultiplyAlpha"; break;
		case BlendMode::MultiplyRGBWithAlphaBlend:	os << "MultiplyRGBWithAlphaBlend"; break;
		case BlendMode::MultiplyRGBAWithAlphaBlend: os << "MultiplyRGBAWithAlphaBlend"; break;
		default:									PTGN_ERROR("Failed to identify blend mode");
	}
	return os;
}

PTGN_SERIALIZER_REGISTER_ENUM(
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