#pragma once

#include <cstdint>

#include "core/math/vector2.h"
#include "core/util/entity_handle.h"
#include "renderer/resources/id.h"
#include "renderer/resources/resource.h"

namespace ptgn {

class AssetManager;

namespace impl {

struct TextureTag {};

using TextureId = Id<TextureTag>;

} // namespace impl

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
		using enum TextureFormat;
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
		using enum TextureFormat;
		case RGBA16F:
		case RGBA32F:
		case R11G11B10F: return true;
		default:		 return false;
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

namespace impl {

class TextureObject : public Resource<TextureId> {
public:
	using Base = Resource<TextureId>;
	using Base::Base;

	V2_int GetSize() const;

	TextureFormat GetFormat() const;
};

} // namespace impl

class Texture : public EntityHandle {
public:
	using EntityHandle::EntityHandle;

	V2_int GetSize() const;
	TextureFormat GetFormat() const;

	operator impl::TextureId() const;

private:
	friend class AssetManager;
};

} // namespace ptgn