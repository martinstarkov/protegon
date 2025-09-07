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

	None,
	/**< No blending:
		 dstRGBA = srcRGBA */

	BlendPremultiplied,
	/**< Premultiplied alpha blending:
		 dstRGBA = srcRGBA + dstRGBA * (1 - srcA) */

	Add,
	/**< Additive blending:
		 dstRGB = srcRGB * srcA + dstRGB
		 dstA   = dstA */

	AddWithAlpha,
	/**< Additive blending with alpha accumulation:
		 dstRGB = srcRGB * srcA + dstRGB
		 dstA   = srcA + dstA */

	AddPremultiplied,
	/**< Premultiplied additive blending:
		 dstRGB = srcRGB + dstRGB
		 dstA   = dstA */

	AddPremultipliedWithAlpha,
	/**< Premultiplied additive blending with alpha accumulation:
		 dstRGB = srcRGB + dstRGB
		 dstA   = srcA + dstA */

	Modulate,
	/**< Color modulation:
		 dstRGB = srcRGB * dstRGB
		 dstA   = dstA */

	MultiplyWithAlphaBlend,
	/**< Color multiply:
		 dstRGB = srcRGB * dstRGB + dstRGB * (1 - srcA)
		 dstA   = dstA */

	Multiply,
	/**< Color multiply:
		 dstRGB = srcRGB * dstRGB
		 dstA   = dstA */

	Stencil
	/**< Stencil multiply:
		 dstRGB = dstRGB
		 dstA   = srcA */
};

inline std::ostream& operator<<(std::ostream& os, BlendMode blend_mode) {
	switch (blend_mode) {
		case BlendMode::Blend:					   os << "Blend"; break;
		case BlendMode::BlendPremultiplied:		   os << "BlendPremultiplied"; break;
		case BlendMode::Add:					   os << "Add"; break;
		case BlendMode::AddPremultiplied:		   os << "AddPremultiplied"; break;
		case BlendMode::AddWithAlpha:			   os << "AddWithAlpha"; break;
		case BlendMode::AddPremultipliedWithAlpha: os << "AddPremultipliedWithAlpha"; break;
		case BlendMode::Modulate:				   os << "Modulate"; break;
		case BlendMode::Multiply:				   os << "Multiply"; break;
		case BlendMode::MultiplyWithAlphaBlend:	   os << "MultiplyWithAlphaBlend"; break;
		case BlendMode::Stencil:				   os << "Stencil"; break;
		case BlendMode::None:					   os << "None"; break;
		default:								   PTGN_ERROR("Failed to identify blend mode");
	}
	return os;
}

PTGN_SERIALIZER_REGISTER_ENUM(
	BlendMode, { { BlendMode::Blend, "blend" },
				 { BlendMode::None, "none" },
				 { BlendMode::BlendPremultiplied, "blend_premultiplied" },
				 { BlendMode::Add, "add" },
				 { BlendMode::AddWithAlpha, "add_with_alpha" },
				 { BlendMode::AddPremultiplied, "add_premultiplied" },
				 { BlendMode::AddPremultipliedWithAlpha, "add_premultiplied_with_alpha" },
				 { BlendMode::Modulate, "modulate" },
				 { BlendMode::Multiply, "multiply" },
				 { BlendMode::Stencil, "stencil" },
				 { BlendMode::MultiplyWithAlphaBlend, "multiply_with_alpha_blend" } }
);

} // namespace ptgn