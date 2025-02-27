#pragma once

#include <iosfwd>
#include <ostream>

#include "utility/log.h"

namespace ptgn {

enum class BlendMode {
	Blend, /**< alpha blending: dstRGB = (srcRGB * srcA) + (dstRGB * (1-srcA)), dstA = srcA + (dstA
			* (1-srcA)) */
	None,  /**< no blending: dstRGBA = srcRGBA */
	BlendPremultiplied, /**< pre-multiplied alpha blending: dstRGBA = srcRGBA + (dstRGBA * (1-srcA))
						 */
	Add,				/**< additive blending: dstRGB = (srcRGB * srcA) + dstRGB, dstA = dstA */
	AddPremultiplied,	/**< pre-multiplied additive blending: dstRGB = srcRGB + dstRGB, dstA = dstA
						 */
	Modulate,			/**< color modulate: dstRGB = srcRGB * dstRGB, dstA = dstA */
	Multiply, /**< color multiply: dstRGB = (srcRGB * dstRGB) + (dstRGB * (1-srcA)), dstA = dstA */
	Stencil	  /**< TOOD: Add explanation */
};

inline std::ostream& operator<<(std::ostream& os, BlendMode blend_mode) {
	switch (blend_mode) {
		case BlendMode::Blend:				os << "Blend"; break;
		case BlendMode::BlendPremultiplied: os << "BlendPremultiplied"; break;
		case BlendMode::Add:				os << "Add"; break;
		case BlendMode::AddPremultiplied:	os << "AddPremultiplied"; break;
		case BlendMode::Modulate:			os << "Modulate"; break;
		case BlendMode::Multiply:			os << "Multiply"; break;
		case BlendMode::Stencil:			os << "Stencil"; break;
		case BlendMode::None:				os << "None"; break;
		default:							PTGN_ERROR("Failed to identify blend mode");
	}
	return os;
}

} // namespace ptgn