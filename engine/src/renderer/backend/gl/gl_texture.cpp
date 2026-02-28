#include "renderer/backend/gl/gl_texture.h"

#include <ostream>
#include <utility>

#include "core/assert.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "core/util/id_map.h"
#include "renderer/backend/gl/gl.h"
#include "renderer/backend/gl/gl_context.h"
#include "renderer/backend/gl/gl_debug.h"
#include "renderer/backend/gl/gl_framebuffer.h"
#include "renderer/resources/id.h"
#include "renderer/resources/texture.h"

namespace ptgn::impl::gl {

Textures::Textures(GLContext& gl) : gl_{ gl } {}

TextureFormat TextureCache::GetFormat() const {
	return format;
}

TextureId Textures::CreateTexture(
	const void* pixel_data, PixelDataFormat pixel_data_format, PixelDataType pixel_data_type,
	V2_int size, TextureFormat format, bool restore_bind
) {
	auto texture{ CreateTexture() };

	auto _ = gl_.Bind(texture, restore_bind);

	SetTextureData(texture, pixel_data, pixel_data_format, pixel_data_type, size, format);

	constexpr TextureMinFilter min_filter{ TextureMinFilter::Nearest };
	constexpr TextureMagFilter mag_filter{ TextureMagFilter::Nearest };
	constexpr TextureWrap wrap_s{ TextureWrap::ClampToEdge };
	constexpr TextureWrap wrap_t{ TextureWrap::ClampToEdge };

	using enum TextureParameter;

	SetTextureParameter(texture, MinFilter, std::to_underlying(min_filter));
	SetTextureParameter(texture, MagFilter, std::to_underlying(mag_filter));
	SetTextureParameter(texture, WrapS, std::to_underlying(wrap_s));
	SetTextureParameter(texture, WrapT, std::to_underlying(wrap_t));

	return texture;
}

V2_int Textures::GetTextureSize(TextureId texture) const {
	PTGN_ASSERT(cache_.Has(texture), "TextureId not in cache");
	return cache_.Get(texture).size;
}

void Textures::ResizeTexture(TextureId texture, V2_int new_size) {
	PTGN_ASSERT(texture);

	const auto& cache = cache_.Get(texture);

	if (cache.size == new_size) {
		return;
	}

	auto _ = gl_.Bind(texture, true);

	SetTextureData(
		texture, nullptr, PixelDataFormat::RGBA, PixelDataType::UnsignedByte, new_size, cache.format
	);
}

TextureCache& Textures::GetCache(TextureId texture) {
	PTGN_ASSERT(cache_.Has(texture), "No texture with id ", texture, " in cache");
	return cache_.Get(texture);
}

const TextureCache& Textures::GetCache(TextureId texture) const {
	PTGN_ASSERT(cache_.Has(texture), "No texture with id ", texture, " in cache");
	return cache_.Get(texture);
}

void Textures::SetTextureData(
	TextureId texture, const void* pixel_data, PixelDataFormat pixel_data_format,
	PixelDataType pixel_data_type, V2_int size, TextureFormat format
) {
	PTGN_ASSERT(gl_.IsBound(texture), "TextureId must be bound prior to setting its data");

	constexpr int mipmap_level{ 0 };
	constexpr int border{ 0 };
	constexpr AttachmentObject target{ AttachmentObject::Texture2D };

	GLCall(glTexImage2D(
		std::to_underlying(target), mipmap_level, std::to_underlying(format), size.x, size.y,
		border, std::to_underlying(pixel_data_format), std::to_underlying(pixel_data_type),
		pixel_data
	));
#ifdef PTGN_GL_DEBUG_TEXTURES
	PTGN_LOG(
		"glTexImage2D(target=", target, ",mipmap_level=", mipmap_level, ",format=", format,
		",size=", size, ",border=", border, ",pixel_format=", pixel_data_format,
		",pixel_type=", pixel_data_type, ",pixel_data=", pixel_data, ")"
	);
#endif

	auto& cache	 = cache_.Get(texture);
	cache.size	 = size;
	cache.format = format;
}

void Textures::SetTextureSubData(
	TextureId texture, const void* pixel_subdata, PixelDataFormat pixel_data_format,
	PixelDataType pixel_data_type, V2_int subdata_size, V2_int subdata_offset
) const {
	PTGN_ASSERT(gl_.IsBound(texture), "TextureId must be bound prior to setting its subdata");
	PTGN_ASSERT(pixel_subdata != nullptr, "Cannot set texture subdata to nullptr");

	constexpr GLint mipmap_level{ 0 };
	constexpr AttachmentObject target{ AttachmentObject::Texture2D };

	GLCall(glTexSubImage2D(
		std::to_underlying(target), mipmap_level, subdata_offset.x, subdata_offset.y,
		subdata_size.x, subdata_size.y, std::to_underlying(pixel_data_format),
		std::to_underlying(pixel_data_type), pixel_subdata
	));
#ifdef PTGN_GL_DEBUG_TEXTURES
	PTGN_LOG(
		"glTexSubImage2D(target=", target, ",mipmap_level=", mipmap_level,
		",offset=", subdata_offset, ",size=", subdata_size, ",pixel_format=", pixel_data_format,
		",pixel_type=", pixel_data_type, ",pixel_subdata=", pixel_subdata, ")"
	);
#endif
}

void Textures::SetTextureParameter(TextureId texture, TextureParameter param, const float* values)
	const {
	PTGN_ASSERT(gl_.IsBound(texture), "TextureId must be bound prior to setting its parameters");
	PTGN_ASSERT(values != nullptr, "Cannot set texture parameter values to nullptr");
	constexpr AttachmentObject target{ AttachmentObject::Texture2D };
	GLCall(glTexParameterfv(std::to_underlying(target), std::to_underlying(param), values));
#ifdef PTGN_GL_DEBUG_TEXTURES
	PTGN_LOG("glTexParameterfv(target=", target, ",param=", param, ",values=", values, ")");
#endif
}

void Textures::SetTextureParameter(TextureId texture, TextureParameter param, const int* values)
	const {
	PTGN_ASSERT(gl_.IsBound(texture), "TextureId must be bound prior to setting its parameters");
	PTGN_ASSERT(values != nullptr, "Cannot set texture parameter values to nullptr");
	constexpr AttachmentObject target{ AttachmentObject::Texture2D };
	GLCall(glTexParameteriv(std::to_underlying(target), std::to_underlying(param), values));
#ifdef PTGN_GL_DEBUG_TEXTURES
	PTGN_LOG("glTexParameteriv(target=", target, ",param=", param, ",values=", values, ")");
#endif
}

void Textures::SetTextureParameter(TextureId texture, TextureParameter param, float value) const {
	PTGN_ASSERT(gl_.IsBound(texture), "TextureId must be bound prior to setting its parameters");
	PTGN_ASSERT(value != -1, "Cannot set texture parameter value to -1");
	constexpr AttachmentObject target{ AttachmentObject::Texture2D };
	GLCall(glTexParameterf(std::to_underlying(target), std::to_underlying(param), value));
#ifdef PTGN_GL_DEBUG_TEXTURES
	PTGN_LOG("glTexParameterf(target=", target, ",param=", param, ",value=", value, ")");
#endif
}

void Textures::SetTextureParameter(TextureId texture, TextureParameter param, int value) const {
	PTGN_ASSERT(gl_.IsBound(texture), "TextureId must be bound prior to setting its parameters");
	PTGN_ASSERT(value != -1, "Cannot set texture parameter value to -1");
	constexpr AttachmentObject target{ AttachmentObject::Texture2D };
	GLCall(glTexParameteri(std::to_underlying(target), std::to_underlying(param), value));
#ifdef PTGN_GL_DEBUG_TEXTURES
	PTGN_LOG("glTexParameteri(target=", target, ",param=", param, ",value=", value, ")");
#endif
}

int Textures::GetTextureParameter(TextureId texture, TextureParameter param) const {
	PTGN_ASSERT(gl_.IsBound(texture), "TextureId must be bound prior to getting its parameters");
	GLint value{ -1 };
	constexpr AttachmentObject target{ AttachmentObject::Texture2D };
	GLCall(glGetTexParameteriv(std::to_underlying(target), std::to_underlying(param), &value));
#ifdef PTGN_GL_DEBUG_TEXTURES
	PTGN_LOG("glGetTexParameteriv(target=", target, ",param=", param, ") -> value=", value);
#endif
	PTGN_ASSERT(value != -1, "Failed to retrieve texture parameter");
	return value;
}

bool Textures::SupportsMipmaps(TextureMinFilter texture_min_filter) {
	using enum ptgn::TextureMinFilter;
	return texture_min_filter == LinearMipmapLinear || texture_min_filter == LinearMipmapNearest ||
		   texture_min_filter == NearestMipmapLinear || texture_min_filter == NearestMipmapNearest;
}

void Textures::GenerateMipmaps(TextureId texture) const {
	PTGN_ASSERT(gl_.IsBound(texture), "TextureId must be bound prior to generating mipmaps for it");
#ifndef __EMSCRIPTEN__
	PTGN_ASSERT(
		SupportsMipmaps(
			static_cast<TextureMinFilter>(GetTextureParameter(texture, TextureParameter::MinFilter))
		),
		"Set texture minifying scaling to mipmap type before generating mipmaps"
	);
#endif
	constexpr AttachmentObject target{ AttachmentObject::Texture2D };
	GLCall(GenerateMipmap(std::to_underlying(target)));
#ifdef PTGN_GL_DEBUG_TEXTURES
	PTGN_LOG("glGenerateMipmap(target=", target, ")");
#endif
}

TextureId Textures::CreateTexture() {
	TextureId id{ 0 };
	GLCall(glGenTextures(1, &id.value));
#ifdef PTGN_GL_DEBUG_TEXTURES
	PTGN_LOG("glGenTextures() -> id=", id.value);
#endif
	PTGN_ASSERT(id, "Failed to create texture");
	cache_.Add(id, TextureCache{});
	return id;
}

void Textures::DestroyTexture(TextureId id) {
	if (!id) {
		return;
	}
	GLCall(glDeleteTextures(1, &id.value));
#ifdef PTGN_GL_DEBUG_TEXTURES
	PTGN_LOG("glDeleteTextures(id=", id.value, ")");
#endif
	cache_.Remove(id);
}

std::ostream& operator<<(std::ostream& os, PixelDataFormat fmt) {
	switch (fmt) {
		using enum PixelDataFormat;
		case RED:			 return os << "RED";
		case RED_INTEGER:	 return os << "RED_INTEGER";
		case RG:			 return os << "RG";
		case RG_INTEGER:	 return os << "RG_INTEGER";
		case RGB:			 return os << "RGB";
		case RGB_INTEGER:	 return os << "RGB_INTEGER";
		case RGBA:			 return os << "RGBA";
		case RGBA_INTEGER:	 return os << "RGBA_INTEGER";
		case DepthComponent: return os << "DepthComponent";
		case DepthStencil:	 return os << "DepthStencil";
		case Stencil:		 return os << "Stencil";
		case LuminanceAlpha: return os << "LuminanceAlpha";
		case Luminance:		 return os << "Luminance";
		case Alpha:			 return os << "Alpha";
		default:			 PTGN_ERROR("Unknown PixelDataFormat: ", std::to_underlying(fmt));
	}
}

std::ostream& operator<<(std::ostream& os, PixelDataType type) {
	switch (type) {
		using enum PixelDataType;
		case UnsignedByte:	   return os << "UnsignedByte";
		case Byte:			   return os << "Byte";
		case UnsignedShort:	   return os << "UnsignedShort";
		case Short:			   return os << "Short";
		case UnsignedInt:	   return os << "UnsignedInt";
		case Int:			   return os << "Int";
		case HalfFloat:		   return os << "HalfFloat";
		case Float:			   return os << "Float";
		case UnsignedInt_24_8: return os << "UnsignedInt_24_8";
		default:			   PTGN_ERROR("Unknown PixelDataType: ", std::to_underlying(type));
	}
}

std::ostream& operator<<(std::ostream& os, TextureParameter param) {
	switch (param) {
		using enum TextureParameter;
		case MinFilter: return os << "MinFilter";
		case MagFilter: return os << "MagFilter";
		case WrapS:		return os << "WrapS";
		case WrapT:		return os << "WrapT";
		default:		PTGN_ERROR("Unknown TextureParameter: ", std::to_underlying(param));
	}
}

} // namespace ptgn::impl::gl