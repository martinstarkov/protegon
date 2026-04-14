#pragma once

#include <cstdint>

#include "serialization/serialize.h"

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
PTGN_REFLECT_ENUM(TextureFormat);

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
PTGN_REFLECT_ENUM(TextureMinFilter);

// Texture Magnification Filter (GL_TEXTURE_MAG_FILTER)
enum class TextureMagFilter : std::int32_t {
	Nearest = 0x2600, // GL_NEAREST
	Linear	= 0x2601  // GL_LINEAR
};
PTGN_REFLECT_ENUM(TextureMagFilter);

// Texture Wrap Mode (GL_TEXTURE_WRAP_S / GL_TEXTURE_WRAP_T)
enum class TextureWrap : std::int32_t {
	Repeat		   = 0x2901, // GL_REPEAT
	MirroredRepeat = 0x8370, // GL_MIRRORED_REPEAT
	ClampToEdge	   = 0x812F	 // GL_CLAMP_TO_EDGE
};
PTGN_REFLECT_ENUM(TextureWrap);

} // namespace ptgn