#pragma once

#include <cstdint>
#include <ostream>
#include <utility>

#include "core/log.h"

namespace ptgn {

enum class TextureFormat : std::uint32_t {
	R8				  = 0x8229, // GL_R8
	RG8				  = 0x822B, // GL_RG8
	RGB8			  = 0x8051, // GL_RGB8
	RGBA8			  = 0x8058, // GL_RGBA8
	R16F			  = 0x822D, // GL_R16F
	RG16F			  = 0x822F, // GL_RG16F
	RGB16F			  = 0x881B, // GL_RGB16F
	RGBA16F			  = 0x881A, // GL_RGBA16F
	R32F			  = 0x822E, // GL_R32F
	RG32F			  = 0x8230, // GL_RG32F
	RGB32F			  = 0x8815, // GL_RGB32F
	RGBA32F			  = 0x8814, // GL_RGBA32F
	Depth16			  = 0x81A5, // GL_DEPTH_COMPONENT16
	Depth24			  = 0x81A6, // GL_DEPTH_COMPONENT24
	Depth32F		  = 0x8CAC, // GL_DEPTH_COMPONENT32F
	Depth24_Stencil8  = 0x88F0, // GL_DEPTH24_STENCIL8
	Depth32F_Stencil8 = 0x8CAD, // GL_DEPTH32F_STENCIL8
	Stencil8		  = 0x8D48, // GL_STENCIL_INDEX8
	SRGB8			  = 0x8C41, // GL_SRGB8
	SRGB8_ALPHA8	  = 0x8C43	// GL_SRGB8_ALPHA8
};

inline bool IsDepthOnlyFormat(TextureFormat fmt) {
	switch (fmt) {
		using enum TextureFormat;
		case Depth16:
		case Depth24:
		case Depth32F: return true;
		default:	   return false;
	}
}

inline bool IsStencilOnlyFormat(TextureFormat fmt) {
	return fmt == TextureFormat::Stencil8;
}

inline bool IsDepthFormat(TextureFormat fmt) {
	switch (fmt) {
		using enum TextureFormat;
		case Depth16:
		case Depth24:
		case Depth32F:
		case Depth24_Stencil8:
		case Depth32F_Stencil8: return true;
		default:				return false;
	}
}

inline bool IsStencilFormat(TextureFormat fmt) {
	switch (fmt) {
		using enum TextureFormat;
		case Stencil8:
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
		using enum TextureFormat;
		case RGBA16F:
		case RGBA32F:
		case RGB16F:
		case RGB32F:
		case RG16F:
		case RG32F:
		case R16F:
		case R32F:	  return true;
		default:	  return false;
	}
}

// Texture Minification Filter (GL_TEXTURE_MIN_FILTER)
enum class TextureMinFilter : std::int32_t {
	Nearest				 = 0x2600, // GL_NEAREST
	Linear				 = 0x2601, // GL_LINEAR
	NearestMipmapNearest = 0x2700, // GL_NEAREST_MIPMAP_NEAREST
	LinearMipmapNearest	 = 0x2701, // GL_LINEAR_MIPMAP_NEAREST
	NearestMipmapLinear	 = 0x2702, // GL_NEAREST_MIPMAP_LINEAR
	LinearMipmapLinear	 = 0x2703  // GL_LINEAR_MIPMAP_LINEAR
};

// Texture Magnification Filter (GL_TEXTURE_MAG_FILTER)
enum class TextureMagFilter : std::int32_t {
	Nearest = 0x2600, // GL_NEAREST
	Linear	= 0x2601  // GL_LINEAR
};

// Texture Wrap Mode (GL_TEXTURE_WRAP_S / GL_TEXTURE_WRAP_T)
enum class TextureWrap : std::int32_t {
	Repeat		   = 0x2901, // GL_REPEAT
	MirroredRepeat = 0x8370, // GL_MIRRORED_REPEAT
	ClampToEdge	   = 0x812F	 // GL_CLAMP_TO_EDGE
};

inline std::ostream& operator<<(std::ostream& os, TextureFormat fmt) {
	switch (fmt) {
		using enum TextureFormat;
		case R8:				return os << "R8";
		case RG8:				return os << "RG8";
		case RGB8:				return os << "RGB8";
		case RGBA8:				return os << "RGBA8";
		case R16F:				return os << "R16F";
		case RG16F:				return os << "RG16F";
		case RGB16F:			return os << "RGB16F";
		case RGBA16F:			return os << "RGBA16F";
		case R32F:				return os << "R32F";
		case RG32F:				return os << "RG32F";
		case RGB32F:			return os << "RGB32F";
		case RGBA32F:			return os << "RGBA32F";
		case Depth16:			return os << "Depth16";
		case Depth24:			return os << "Depth24";
		case Depth32F:			return os << "Depth32F";
		case Depth24_Stencil8:	return os << "Depth24_Stencil8";
		case Depth32F_Stencil8: return os << "Depth32F_Stencil8";
		case Stencil8:			return os << "Stencil8";
		case SRGB8:				return os << "SRGB8";
		case SRGB8_ALPHA8:		return os << "SRGB8_ALPHA8";
		default:				PTGN_ERROR("Unknown texture format: ", std::to_underlying(fmt));
	}
}

inline std::ostream& operator<<(std::ostream& os, TextureMinFilter filter) {
	switch (filter) {
		using enum TextureMinFilter;
		case Nearest:			   return os << "Nearest";
		case Linear:			   return os << "Linear";
		case NearestMipmapNearest: return os << "NearestMipmapNearest";
		case LinearMipmapNearest:  return os << "LinearMipmapNearest";
		case NearestMipmapLinear:  return os << "NearestMipmapLinear";
		case LinearMipmapLinear:   return os << "LinearMipmapLinear";
		default:				   PTGN_ERROR("Unknown texture min filter: ", std::to_underlying(filter));
	}
}

inline std::ostream& operator<<(std::ostream& os, TextureMagFilter filter) {
	switch (filter) {
		using enum TextureMagFilter;
		case Nearest: return os << "Nearest";
		case Linear:  return os << "Linear";
		default:	  PTGN_ERROR("Unknown texture mag filter: ", std::to_underlying(filter));
	}
}

inline std::ostream& operator<<(std::ostream& os, TextureWrap wrap) {
	switch (wrap) {
		using enum TextureWrap;
		case ClampToEdge:	 return os << "ClampToEdge";
		case MirroredRepeat: return os << "MirroredRepeat";
		case Repeat:		 return os << "Repeat";
		default:			 PTGN_ERROR("Unknown texture wrap: ", std::to_underlying(wrap));
	}
}

} // namespace ptgn