#pragma once

namespace ptgn {

enum class TextureFormat {
	// ---------------------------------------------------------------------
	// Color (8-bit normalized)
	// ---------------------------------------------------------------------
	R8,
	RG8,
	RGBA8,

	RGBA8_SRGB,

	// ---------------------------------------------------------------------
	// Color (16-bit / float)
	// ---------------------------------------------------------------------
	R16F,
	RG16F,
	RGBA16F,

	R32F,
	RG32F,
	RGBA32F,

	// ---------------------------------------------------------------------
	// HDR / lighting
	// ---------------------------------------------------------------------
	RGB10_A2,
	R11G11B10F,

	// ---------------------------------------------------------------------
	// Depth / stencil
	// ---------------------------------------------------------------------
	Depth16,
	Depth24,
	Depth32F,

	Depth24_Stencil8,
	Depth32F_Stencil8,

	// ---------------------------------------------------------------------
	// Special / utility
	// ---------------------------------------------------------------------
	Stencil8
};

inline bool IsDepthFormat(TextureFormat fmt) {
	switch (fmt) {
		using enum ptgn::TextureFormat;
		case Depth16:
		case Depth24:
		case Depth32F:
		case Depth24_Stencil8:
		case Depth32F_Stencil8: return true;
		default:				return false;
	}
}

inline bool IsColorFormat(TextureFormat fmt) {
	return !IsDepthFormat(fmt) && fmt != TextureFormat::Stencil8;
}

inline bool IsHDRFormat(TextureFormat fmt) {
	switch (fmt) {
		using enum ptgn::TextureFormat;
		case RGBA16F:
		case RGBA32F:
		case R11G11B10F: return true;
		default:		 return false;
	}
}

} // namespace ptgn