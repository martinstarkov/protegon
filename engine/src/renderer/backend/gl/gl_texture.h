#pragma once

#include "core/graphics/color.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "core/util/id_map.h"
#include "renderer/backend/gl/gl.h"
#include "renderer/resources/texture.h"

namespace ptgn::impl::gl {

class GLContext;

struct TextureCache {
	V2_int size;
	GLenum internal_format{ GL_RGBA8 };

	TextureFormat GetFormat() const;
};

struct TextureFormatDesc {
	GLenum internal_format{ 0 };
	GLenum pixel_format{ 0 };
	GLenum pixel_type{ 0 };

	bool has_depth{ false };
	bool has_stencil{ false };
	bool is_srgb{ false };
};

constexpr TextureFormatDesc GetTextureFormatDesc(TextureFormat fmt) {
	switch (fmt) {
		using enum TextureFormat;

		// -----------------------------------------------------------------
		// Most common color formats (put first)
		// -----------------------------------------------------------------
		case RGBA8:		 return { GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, false, false, false };
		case RGBA8_SRGB: return { GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE, false, false, true };
		case RGBA16F:	 return { GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, false, false, false };
		case RGBA32F:	 return { GL_RGBA32F, GL_RGBA, GL_FLOAT, false, false, false };

		case RG8:		 return { GL_RG8, GL_RG, GL_UNSIGNED_BYTE, false, false, false };
		case R8:		 return { GL_R8, GL_RED, GL_UNSIGNED_BYTE, false, false, false };

		// -----------------------------------------------------------------
		// Color (16-bit / float) - missing before
		// -----------------------------------------------------------------
		case R16F:		 return { GL_R16F, GL_RED, GL_HALF_FLOAT, false, false, false };
		case RG16F:		 return { GL_RG16F, GL_RG, GL_HALF_FLOAT, false, false, false };

		// -----------------------------------------------------------------
		// Color (32-bit float) - missing before
		// -----------------------------------------------------------------
		case R32F:		 return { GL_R32F, GL_RED, GL_FLOAT, false, false, false };
		case RG32F:		 return { GL_RG32F, GL_RG, GL_FLOAT, false, false, false };

		// -----------------------------------------------------------------
		// HDR / lighting
		// -----------------------------------------------------------------
		case R11G11B10F:
			return {
				GL_R11F_G11F_B10F, GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV, false, false, false
			};

		case RGB10_A2:
			return { GL_RGB10_A2, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, false, false, false };

		// -----------------------------------------------------------------
		// Depth / stencil
		// -----------------------------------------------------------------
		case Depth16:
			return {
				GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, true, false, false
			};

		case Depth24:
			return {
				GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, true, false, false
			};

		case Depth32F:
			return { GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT, true, false, false };

		case Depth24_Stencil8:
			return {
				GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, true, true, false
			};

		case Depth32F_Stencil8:
			return { GL_DEPTH32F_STENCIL8,
					 GL_DEPTH_STENCIL,
					 GL_FLOAT_32_UNSIGNED_INT_24_8_REV,
					 true,
					 true,
					 false };

		case Stencil8:
			return { GL_STENCIL_INDEX8, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, false, true, false };

		default:
			// No UB / no exceptions:
			PTGN_ERROR("Unknown TextureFormat");
	}
}

constexpr TextureFormat GetTextureFormatFromInternal(GLenum internal) {
	switch (internal) {
		using enum TextureFormat;
		// -----------------------------------------------------------------
		// Most common color formats
		// -----------------------------------------------------------------
		case GL_RGBA8:				return RGBA8;
		case GL_SRGB8_ALPHA8:		return RGBA8_SRGB;
		case GL_RGBA16F:			return RGBA16F;
		case GL_RGBA32F:			return RGBA32F;

		case GL_RG8:				return RG8;
		case GL_R8:					return R8;

		// -----------------------------------------------------------------
		// 16-bit float
		// -----------------------------------------------------------------
		case GL_R16F:				return R16F;
		case GL_RG16F:				return RG16F;

		// -----------------------------------------------------------------
		// 32-bit float
		// -----------------------------------------------------------------
		case GL_R32F:				return R32F;
		case GL_RG32F:				return RG32F;

		// -----------------------------------------------------------------
		// HDR / packed
		// -----------------------------------------------------------------
		case GL_R11F_G11F_B10F:		return R11G11B10F;
		case GL_RGB10_A2:			return RGB10_A2;

		// -----------------------------------------------------------------
		// Depth / stencil
		// -----------------------------------------------------------------
		case GL_DEPTH_COMPONENT16:	return Depth16;
		case GL_DEPTH_COMPONENT24:	return Depth24;
		case GL_DEPTH_COMPONENT32F: return Depth32F;

		case GL_DEPTH24_STENCIL8:	return Depth24_Stencil8;
		case GL_DEPTH32F_STENCIL8:	return Depth32F_Stencil8;

		case GL_STENCIL_INDEX8:		return Stencil8;

		default:					PTGN_ERROR("Unknown internal format");
	}
}

[[nodiscard]] constexpr int GetColorComponentCount(GLenum internal_format) {
	switch (internal_format) {
		case GL_STENCIL_INDEX:	 return 1; // stencil only
		case GL_DEPTH_COMPONENT: return 1; // depth only
		case GL_DEPTH_STENCIL:	 return 2; // depth + stencil

		case GL_RED:			 return 1;
		case GL_GREEN:			 return 1;
		case GL_BLUE:			 return 1;

		case GL_RG:				 return 2; // red + green
		case GL_RGB:			 return 3; // red + green + blue
		case GL_BGR:			 return 3; // blue + green + red (different order)
		case GL_RGBA:			 return 4; // red + green + blue + alpha
		case GL_BGRA:			 return 4; // blue + green + red + alpha (different order)

		default:				 PTGN_ERROR("Unknown or unsupported internal GL format: ", internal_format);
	}
}

class Textures {
public:
	/// @param pixel_data_format Accepted: GL_RED, GL_RG, GL_RGB, GL_BGR, GL_RGBA, GL_BGRA,
	/// GL_RED_INTEGER, GL_RG_INTEGER, GL_RGB_INTEGER, GL_BGR_INTEGER, GL_RGBA_INTEGER,
	/// GL_BGRA_INTEGER, GL_STENCIL_INDEX, GL_DEPTH_COMPONENT, GL_DEPTH_STENCIL
	/// @param internal_format Accepted: GL_RGBA, GL_RGB, GL_RG, GL_RED, GL_DEPTH_STENCIL,
	/// GL_DEPTH_COMPONENT
	Texture CreateTexture(
		const void* pixel_data, GLenum pixel_data_format, GLenum pixel_data_type, V2_int size,
		GLenum internal_format, bool restore_bind = true
	);

	void DestroyTexture(Texture id);

	V2_int GetTextureSize(Texture texture) const;

	void ResizeTexture(Texture texture, V2_int new_size);

	TextureCache& GetCache(Texture texture);
	const TextureCache& GetCache(Texture texture) const;

private:
	friend class GLContext;

	explicit Textures(GLContext& gl);
	~Textures() noexcept					 = default;
	Textures(const Textures&)				 = delete;
	Textures(Textures&&) noexcept			 = delete;
	Textures& operator=(const Textures&)	 = delete;
	Textures& operator=(Textures&&) noexcept = delete;

	/// @param pixel_data_format Accepted: GL_RED, GL_RG, GL_RGB, GL_BGR, GL_RGBA, GL_BGRA,
	/// GL_RED_INTEGER, GL_RG_INTEGER, GL_RGB_INTEGER, GL_BGR_INTEGER, GL_RGBA_INTEGER,
	/// GL_BGRA_INTEGER, GL_STENCIL_INDEX, GL_DEPTH_COMPONENT, GL_DEPTH_STENCIL
	/// @param internal_format Accepted: GL_RGBA, GL_RGB, GL_RG, GL_RED, GL_DEPTH_STENCIL,
	/// GL_DEPTH_COMPONENT
	void SetTextureData(
		Texture texture, const void* pixel_data, GLenum pixel_data_format, GLenum pixel_data_type,
		V2_int size, GLenum internal_format
	);

	void SetTextureSubData(
		Texture texture, const void* pixel_subdata, GLenum pixel_data_format,
		GLenum pixel_data_type, V2_int subdata_size, V2_int subdata_offset
	) const;

	void SetTextureClampBorderColor(Texture texture, Color color) const;

	void SetTextureParameter(Texture texture, GLenum param, const GLfloat* values) const;
	void SetTextureParameter(Texture texture, GLenum param, const GLint* values) const;
	void SetTextureParameter(Texture texture, GLenum param, GLfloat value) const;
	void SetTextureParameter(Texture texture, GLenum param, GLint value) const;

	[[nodiscard]] GLint GetTextureParameter(Texture texture, GLenum param) const;

	/// Ensure that the texture scaling of the currently bound texture is valid for generating
	/// mipmaps.
	[[nodiscard]] static bool SupportsMipmaps(GLenum texture_min_filter);

	void GenerateMipmaps(Texture texture) const;

	[[nodiscard]] Texture CreateTexture();

	GLContext& gl_;

	IdMap<TextureCache> cache_;
};

} // namespace ptgn::impl::gl