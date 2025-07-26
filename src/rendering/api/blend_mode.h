#pragma once

#include <iosfwd>
#include <ostream>

#include "debug/log.h"

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

	Multiply,
	/**< Color multiply:
		 dstRGB = srcRGB * dstRGB + dstRGB * (1 - srcA)
		 dstA   = dstA */
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
		// case BlendMode::Stencil:			os << "Stencil"; break; // TODO: Readd.
		case BlendMode::None:					   os << "None"; break;
		default:								   PTGN_ERROR("Failed to identify blend mode");
	}
	return os;
}

} // namespace ptgn