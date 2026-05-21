#pragma once

#include <cstdint>
#include <string_view>
#include <utility>

#include "core/log.h"

namespace ptgn {

/// @brief Texture storage format (GL_INTERNAL_FORMAT)
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

[[nodiscard]] inline std::string_view ToString(TextureFormat format) {
	switch (format) {
		using enum TextureFormat;
		case R8:				return "R8";
		case RG8:				return "RG8";
		case RGB8:				return "RGB8";
		case RGBA8:				return "RGBA8";
		case R16F:				return "R16F";
		case RG16F:				return "RG16F";
		case RGB16F:			return "RGB16F";
		case RGBA16F:			return "RGBA16F";
		case R32F:				return "R32F";
		case RG32F:				return "RG32F";
		case RGB32F:			return "RGB32F";
		case RGBA32F:			return "RGBA32F";
		case Depth16:			return "Depth16";
		case Depth24:			return "Depth24";
		case Depth32F:			return "Depth32F";
		case Depth24_Stencil8:	return "Depth24_Stencil8";
		case Depth32F_Stencil8: return "Depth32F_Stencil8";
		case Stencil8:			return "Stencil8";
		case SRGB8:				return "SRGB8";
		case SRGB8_ALPHA8:		return "SRGB8_ALPHA8";
	}

	return "UnknownTextureFormat";
}

/// @brief Texture Minification Filter (GL_TEXTURE_MIN_FILTER)
enum class TextureMinFilter : std::int32_t {
	Nearest				 = 0x2600, // GL_NEAREST
	Linear				 = 0x2601, // GL_LINEAR
	NearestMipmapNearest = 0x2700, // GL_NEAREST_MIPMAP_NEAREST
	LinearMipmapNearest	 = 0x2701, // GL_LINEAR_MIPMAP_NEAREST
	NearestMipmapLinear	 = 0x2702, // GL_NEAREST_MIPMAP_LINEAR
	LinearMipmapLinear	 = 0x2703  // GL_LINEAR_MIPMAP_LINEAR
};

/// @brief Texture Magnification Filter (GL_TEXTURE_MAG_FILTER)
enum class TextureMagFilter : std::int32_t {
	Nearest = 0x2600, // GL_NEAREST
	Linear	= 0x2601  // GL_LINEAR
};

/// @brief Texture Wrap Mode (GL_TEXTURE_WRAP_S / GL_TEXTURE_WRAP_T)
enum class TextureWrap : std::int32_t {
	Repeat		   = 0x2901, // GL_REPEAT
	MirroredRepeat = 0x8370, // GL_MIRRORED_REPEAT
	ClampToEdge	   = 0x812F	 // GL_CLAMP_TO_EDGE
};

struct TextureParameters {
	constexpr TextureParameters() = default;

	constexpr TextureParameters(TextureMinFilter min_filter, TextureMagFilter mag_filter) :
		min_filter{ min_filter }, mag_filter{ mag_filter } {}

	constexpr TextureParameters(
		TextureMinFilter min_filter, TextureMagFilter mag_filter, TextureWrap wrap_s,
		TextureWrap wrap_t
	) :
		min_filter{ min_filter }, mag_filter{ mag_filter }, wrap_s{ wrap_s }, wrap_t{ wrap_t } {}

	TextureMinFilter min_filter{ TextureMinFilter::Nearest };
	TextureMagFilter mag_filter{ TextureMagFilter::Nearest };
	TextureWrap wrap_s{ TextureWrap::ClampToEdge };
	TextureWrap wrap_t{ TextureWrap::ClampToEdge };
};

inline int GetChannelCount(TextureFormat format) {
	switch (format) {
		using enum TextureFormat;
		case R8:				[[fallthrough]];
		case R16F:				[[fallthrough]];
		case R32F:				return 1;
		case RG8:				[[fallthrough]];
		case RG16F:				[[fallthrough]];
		case RG32F:				return 2;
		case RGB8:				[[fallthrough]];
		case RGB16F:			[[fallthrough]];
		case RGB32F:			[[fallthrough]];
		case SRGB8:				return 3;
		case RGBA8:				[[fallthrough]];
		case RGBA16F:			[[fallthrough]];
		case RGBA32F:			[[fallthrough]];
		case Depth24_Stencil8:	[[fallthrough]];
		case Depth32F_Stencil8: [[fallthrough]];
		case SRGB8_ALPHA8:		return 4;
		case Depth16:			[[fallthrough]];
		case Depth24:			[[fallthrough]];
		case Depth32F:			return 0; // Depth formats don't have color channels.
		case Stencil8:			return 0; // Stencil formats don't have color channels.
		default:				PTGN_ERROR("Unknown TextureFormat: ", std::to_underlying(format));
	}
}

inline bool IsDepthOnlyFormat(TextureFormat fmt) {
	switch (fmt) {
		using enum TextureFormat;
		case Depth16:  [[fallthrough]];
		case Depth24:  [[fallthrough]];
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
		case Depth16:			[[fallthrough]];
		case Depth24:			[[fallthrough]];
		case Depth32F:			[[fallthrough]];
		case Depth24_Stencil8:	[[fallthrough]];
		case Depth32F_Stencil8: return true;
		default:				return false;
	}
}

inline bool IsStencilFormat(TextureFormat fmt) {
	switch (fmt) {
		using enum TextureFormat;
		case Stencil8:			[[fallthrough]];
		case Depth24_Stencil8:	[[fallthrough]];
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
		case RGBA16F: [[fallthrough]];
		case RGBA32F: [[fallthrough]];
		case RGB16F:  [[fallthrough]];
		case RGB32F:  [[fallthrough]];
		case RG16F:	  [[fallthrough]];
		case RG32F:	  [[fallthrough]];
		case R16F:	  [[fallthrough]];
		case R32F:	  return true;
		default:	  return false;
	}
}

} // namespace ptgn