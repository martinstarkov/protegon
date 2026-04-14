#pragma once

#include <cstdint>
#include <utility>

#include "core/log.h"
#include "core/math/vector2.h"
#include "core/util/id_map.h"
#include "renderer/resources/id.h"
#include "renderer/resources/texture_format.h"
#include "serialization/serialize.h"

namespace ptgn::impl::gl {

class GLContext;

enum class PixelDataFormat : std::uint32_t {
	RED			   = 0x1903, // GL_RED
	RED_INTEGER	   = 0x8D94, // GL_RED_INTEGER
	RG			   = 0x8227, // GL_RG
	RG_INTEGER	   = 0x8228, // GL_RG_INTEGER
	RGB			   = 0x1907, // GL_RGB
	RGB_INTEGER	   = 0x8D98, // GL_RGB_INTEGER
	RGBA		   = 0x1908, // GL_RGBA
	RGBA_INTEGER   = 0x8D99, // GL_RGBA_INTEGER
	DepthComponent = 0x1902, // GL_DEPTH_COMPONENT
	DepthStencil   = 0x84F9, // GL_DEPTH_STENCIL
	Stencil		   = 0x1901, // GL_STENCIL_INDEX
	LuminanceAlpha = 0x190A, // GL_LUMINANCE_ALPHA
	Luminance	   = 0x1909, // GL_LUMINANCE
	Alpha		   = 0x1906	 // GL_ALPHA
};
PTGN_REFLECT_ENUM(PixelDataFormat);

enum class PixelDataType : std::uint32_t {
	UnsignedByte	 = 0x1401, // GL_UNSIGNED_BYTE
	Byte			 = 0x1400, // GL_BYTE
	UnsignedShort	 = 0x1403, // GL_UNSIGNED_SHORT
	Short			 = 0x1402, // GL_SHORT
	UnsignedInt		 = 0x1405, // GL_UNSIGNED_INT
	Int				 = 0x1404, // GL_INT
	HalfFloat		 = 0x140B, // GL_HALF_FLOAT
	Float			 = 0x1406, // GL_FLOAT
	UnsignedInt_24_8 = 0x84FA  // GL_UNSIGNED_INT_24_8
};
PTGN_REFLECT_ENUM(PixelDataType);

enum class TextureParameter : std::uint32_t {
	MinFilter = 0x2801, // GL_TEXTURE_MIN_FILTER
	MagFilter = 0x2800, // GL_TEXTURE_MAG_FILTER
	WrapS	  = 0x2802, // GL_TEXTURE_WRAP_S
	WrapT	  = 0x2803, // GL_TEXTURE_WRAP_T
};
PTGN_REFLECT_ENUM(TextureParameter);

struct TextureCache {
	V2_int size;
	TextureFormat format{ TextureFormat::RGBA8 };
};

constexpr int GetBitCount(TextureFormat fmt) {
	switch (fmt) {
		using enum TextureFormat;
		case RGBA8:
		case RGBA16F:
		case RGBA32F:
		case SRGB8_ALPHA8:		return 4;
		case RGB8:
		case RGB16F:
		case RGB32F:
		case SRGB8:				return 3;
		case R8:
		case R16F:
		case R32F:
		case Depth16:
		case Depth24:
		case Depth32F:
		case Stencil8:			return 1;
		case RG8:
		case RG16F:
		case RG32F:
		case Depth24_Stencil8:
		case Depth32F_Stencil8: return 2;
		default:				PTGN_ERROR("Unknown texture format: ", fmt);
	}
}

constexpr std::pair<PixelDataFormat, PixelDataType> GetPixelDataFormat(TextureFormat fmt) {
	switch (fmt) {
		using enum PixelDataFormat;
		using enum TextureFormat;
		using enum PixelDataType;
		case RGBA8:
		case SRGB8_ALPHA8:		return { RGBA, UnsignedByte };
		case RGBA16F:
		case RGBA32F:			return { RGBA, Float };
		case RGB8:
		case SRGB8:				return { RGB, UnsignedByte };
		case RGB16F:
		case RGB32F:			return { RGB, Float };
		case R16F:
		case R32F:				return { RED, Float };
		case R8:				return { RED, UnsignedByte };
		case Depth16:
		case Depth24:			return { DepthComponent, UnsignedInt };
		case Depth32F:			return { DepthComponent, Float };
		case Stencil8:			return { Stencil, UnsignedByte };
		case Depth24_Stencil8:
		case Depth32F_Stencil8: return { DepthStencil, UnsignedInt_24_8 };
		case RG8:				return { RG, UnsignedByte };
		case RG16F:
		case RG32F:				return { RG, Float };
		default:				PTGN_ERROR("Unknown texture format: ", fmt);
	}
}

class Textures {
public:
	/// @brief Creates an empty texture with the given size and format.
	TextureId CreateTexture(V2_int size, TextureFormat format);

	TextureId CreateTexture(
		const void* pixel_data, PixelDataFormat pixel_data_format, PixelDataType pixel_data_type,
		V2_int size, TextureFormat texture_format, bool restore_bind = true
	);

	void DestroyTexture(TextureId id);

	V2_int GetTextureSize(TextureId texture) const;
	TextureFormat GetTextureFormat(TextureId texture) const;

	void ResizeTexture(TextureId texture, V2_int new_size);

	TextureCache& GetCache(TextureId texture);
	const TextureCache& GetCache(TextureId texture) const;

private:
	friend class GLContext;

	explicit Textures(GLContext& gl);
	~Textures() noexcept					 = default;
	Textures(const Textures&)				 = delete;
	Textures(Textures&&) noexcept			 = delete;
	Textures& operator=(const Textures&)	 = delete;
	Textures& operator=(Textures&&) noexcept = delete;

	void SetTextureData(
		TextureId texture, const void* pixel_data, PixelDataFormat pixel_data_format,
		PixelDataType pixel_data_type, V2_int size, TextureFormat texture_format
	);

	void SetTextureSubData(
		TextureId texture, const void* pixel_subdata, PixelDataFormat pixel_data_format,
		PixelDataType pixel_data_type, V2_int subdata_size, V2_int subdata_offset
	) const;

	void SetTextureParameter(TextureId texture, TextureParameter param, const float* values) const;
	void SetTextureParameter(TextureId texture, TextureParameter param, const int* values) const;
	void SetTextureParameter(TextureId texture, TextureParameter param, float value) const;
	void SetTextureParameter(TextureId texture, TextureParameter param, int value) const;

	int GetTextureParameter(TextureId texture, TextureParameter param) const;

	/// @return True if the texture scaling of the currently bound texture is valid for
	/// generating mipmaps.
	[[nodiscard]] static bool SupportsMipmaps(TextureMinFilter texture_min_filter);

	void GenerateMipmaps(TextureId texture) const;

	[[nodiscard]] TextureId CreateTexture();

	GLContext& gl_;

	IdMap<TextureCache> cache_;
};

} // namespace ptgn::impl::gl