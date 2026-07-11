#pragma once

#include <cstdint>
#include <optional>
#include <utility>

#include "core/log.h"
#include "core/math/vector2.h"
#include "core/util/id_map.h"
#include "renderer/resources/id.h"
#include "renderer/resources/texture.h"
#include "renderer/resources/texture_format.h"

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
};

enum class PixelDataType : std::uint32_t {
	UnsignedByte	 = 0x1401, // GL_UNSIGNED_BYTE
	Byte			 = 0x1400, // GL_BYTE
	UnsignedShort	 = 0x1403, // GL_UNSIGNED_SHORT
	Short			 = 0x1402, // GL_SHORT
	UnsignedInt		 = 0x1405, // GL_UNSIGNED_INT
	Int				 = 0x1404, // GL_INT
	HalfFloat		 = 0x140B, // GL_HALF_FLOAT
	Float			 = 0x1406, // GL_FLOAT
	UnsignedInt_24_8		   = 0x84FA, // GL_UNSIGNED_INT_24_8
	Float32UnsignedInt_24_8Rev = 0x8DAD	 // GL_FLOAT_32_UNSIGNED_INT_24_8_REV
};

enum class TextureParameter : std::uint32_t {
	MinFilter = 0x2801, // GL_TEXTURE_MIN_FILTER
	MagFilter = 0x2800, // GL_TEXTURE_MAG_FILTER
	WrapS	  = 0x2802, // GL_TEXTURE_WRAP_S
	WrapT	  = 0x2803, // GL_TEXTURE_WRAP_T
};

struct TextureCache {
	TextureDesc desc;
};

/// @return Number of logical components represented by the format.
/// This is not the number of color channels or bytes per pixel.
constexpr int GetComponentCount(TextureFormat format) {
	if (IsColorFormat(format)) {
		return GetChannelCount(format);
	}

	if (IsDepthStencilOnlyFormat(format)) {
		return 2;
	}

	if (IsDepthOnlyFormat(format) || IsStencilOnlyFormat(format)) {
		return 1;
	}

	PTGN_ERROR("Unknown TextureFormat: ", std::to_underlying(format));
}

/// @return External pixel transfer format and type used for uploads/readbacks.
/// This does not describe the texture's internal storage size.
constexpr std::pair<PixelDataFormat, PixelDataType> GetPixelDataFormat(TextureFormat fmt) {
	switch (fmt) {
		using enum PixelDataFormat;
		using enum TextureFormat;
		using enum PixelDataType;
		case RGBA8:				[[fallthrough]];
		case SRGB8_ALPHA8:		return { RGBA, UnsignedByte };
		case RGBA16F:			[[fallthrough]];
		case RGBA32F:			return { RGBA, Float };
		case RGB8:				[[fallthrough]];
		case SRGB8:				return { RGB, UnsignedByte };
		case RGB16F:			[[fallthrough]];
		case RGB32F:			return { RGB, Float };
		case R16F:				[[fallthrough]];
		case R32F:				return { RED, Float };
		case R32I:				return { RED_INTEGER, Int };
		case R8:				return { RED, UnsignedByte };
		case Depth16:			[[fallthrough]];
		case Depth24:			return { DepthComponent, UnsignedInt };
		case Depth32F:			return { DepthComponent, Float };
		case Stencil8:			return { Stencil, UnsignedByte };
		case Depth24_Stencil8:	return { DepthStencil, UnsignedInt_24_8 };
		case Depth32F_Stencil8: return { DepthStencil, Float32UnsignedInt_24_8Rev };
		case RG8:				return { RG, UnsignedByte };
		case RG16F:				[[fallthrough]];
		case RG32F:				return { RG, Float };
		default:				PTGN_ERROR("Unknown texture format: ", std::to_underlying(fmt));
	}
}

constexpr std::size_t GetPixelDataTypeSize(PixelDataType type) {
	switch (type) {
		using enum PixelDataType;

		case UnsignedByte:				 [[fallthrough]];
		case Byte:						 return 1;

		case UnsignedShort:				 [[fallthrough]];
		case Short:						 [[fallthrough]];
		case HalfFloat:					 return 2;

		case UnsignedInt:				 [[fallthrough]];
		case Int:						 [[fallthrough]];
		case Float:						 [[fallthrough]];
		case UnsignedInt_24_8:			 return 4;

		case Float32UnsignedInt_24_8Rev: return 8;

		default:						 PTGN_ERROR("Unknown PixelDataType: ", std::to_underlying(type));
	}
}

class Textures {
public:
	/// @brief Creates an empty texture with the given size and format.
	TextureId Create(TextureDesc desc, bool restore_bind);

	TextureId Create(
		const void* pixel_data, PixelDataFormat pixel_data_format, PixelDataType pixel_data_type,
		TextureDesc desc, bool restore_bind
	);

	TextureId Create(const void* pixel_data, TextureDesc desc, bool restore_bind);

	void Destroy(TextureId id, TextureId replacement_texture);

	std::optional<TextureDesc> GetDesc(TextureId texture) const;

	void Resize(TextureId texture, V2_int new_size);

	TextureCache& GetCache(TextureId texture);
	const TextureCache& GetCache(TextureId texture) const;

	void SetParameter(TextureId texture, TextureParameter param, int value);

private:
	friend class GLContext;

	explicit Textures(GLContext& gl);
	~Textures() noexcept					 = default;
	Textures(const Textures&)				 = delete;
	Textures(Textures&&) noexcept			 = delete;
	Textures& operator=(const Textures&)	 = delete;
	Textures& operator=(Textures&&) noexcept = delete;

	void SetData(
		TextureId texture, const void* pixel_data, PixelDataFormat pixel_data_format,
		PixelDataType pixel_data_type, V2_int size, TextureFormat texture_format
	);

	void SetSubData(
		TextureId texture, const void* pixel_subdata, PixelDataFormat pixel_data_format,
		PixelDataType pixel_data_type, V2_int subdata_size, V2_int subdata_offset
	) const;

	void SetParameter(TextureId texture, TextureParameter param, const float* values) const;
	void SetParameter(TextureId texture, TextureParameter param, const int* values) const;
	void SetParameter(TextureId texture, TextureParameter param, float value) const;

	int GetParameter(TextureId texture, TextureParameter param) const;

	/// @return True if the texture scaling of the currently bound texture is valid for
	/// generating mipmaps.
	[[nodiscard]] static bool SupportsMipmaps(TextureMinFilter texture_min_filter);

	void GenerateMipmaps(TextureId texture) const;

	[[nodiscard]] TextureId CreateImpl();

	GLContext& gl_;

	IdMap<TextureCache> cache_;
};

} // namespace ptgn::impl::gl