#include "renderer/backend/gl/gl_texture.h"

#include <utility>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "core/math/vector4.h"
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
	return GetTextureFormatFromInternal(internal_format);
}

TextureId Textures::CreateTexture(
	const void* pixel_data, GLenum pixel_data_format, GLenum pixel_data_type, V2_int size,
	GLenum internal_format, bool restore_bind
) {
	auto texture{ CreateTexture() };

	auto _ = gl_.Bind(texture, restore_bind);

	SetTextureData(texture, pixel_data, pixel_data_format, pixel_data_type, size, internal_format);

	constexpr TextureMinFilter min_filter{ TextureMinFilter::Nearest };
	constexpr TextureMagFilter mag_filter{ TextureMagFilter::Nearest };
	constexpr TextureWrap wrap_s{ TextureWrap::ClampToEdge };
	constexpr TextureWrap wrap_t{ TextureWrap::ClampToEdge };

	SetTextureParameter(texture, GL_TEXTURE_MIN_FILTER, std::to_underlying(min_filter));
	SetTextureParameter(texture, GL_TEXTURE_MAG_FILTER, std::to_underlying(mag_filter));
	SetTextureParameter(texture, GL_TEXTURE_WRAP_S, std::to_underlying(wrap_s));
	SetTextureParameter(texture, GL_TEXTURE_WRAP_T, std::to_underlying(wrap_t));

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

	SetTextureData(texture, nullptr, GL_RGBA, GL_UNSIGNED_BYTE, new_size, cache.internal_format);
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
	TextureId texture, const void* pixel_data, GLenum pixel_data_format, GLenum pixel_data_type,
	V2_int size, GLenum internal_format
) {
	PTGN_ASSERT(gl_.IsBound(texture), "TextureId must be bound prior to setting its data");

	constexpr GLint mipmap_level{ 0 };
	constexpr GLint border{ 0 };

#ifdef __EMSCRIPTEN__
	PTGN_ASSERT(
		pixel_data_format != GL_BGRA && pixel_data_format != GL_BGR && internal_format != GL_BGRA &&
			internal_format != GL_BGR,
		"OpenGL ES3.0 does not support BGR(A) formats in glTexImage2D"
	);
#endif

	constexpr AttachmentObject target{ AttachmentObject::Texture2D };

	GLCall(glTexImage2D(
		std::to_underlying(target), mipmap_level, internal_format, size.x, size.y, border,
		pixel_data_format, pixel_data_type, pixel_data
	));
#ifdef GL_DEBUG_TEXTURES
	PTGN_LOG(
		"glTexImage2D(target=", target, ",mipmap_level=", mipmap_level,
		",internal_format=", internal_format, ",size=", size, ",border=", border,
		",pixel_format=", pixel_data_format, ",pixel_type=", pixel_data_type,
		",pixel_data=", pixel_data, ")"
	);
#endif

	auto& cache			  = cache_.Get(texture);
	cache.size			  = size;
	cache.internal_format = internal_format;
}

void Textures::SetTextureSubData(
	TextureId texture, const void* pixel_subdata, GLenum pixel_data_format, GLenum pixel_data_type,
	V2_int subdata_size, V2_int subdata_offset
) const {
	PTGN_ASSERT(gl_.IsBound(texture), "TextureId must be bound prior to setting its subdata");
	PTGN_ASSERT(pixel_subdata != nullptr, "Cannot set texture subdata to nullptr");

	constexpr GLint mipmap_level{ 0 };
	constexpr AttachmentObject target{ AttachmentObject::Texture2D };

	GLCall(glTexSubImage2D(
		std::to_underlying(target), mipmap_level, subdata_offset.x, subdata_offset.y,
		subdata_size.x, subdata_size.y, pixel_data_format, pixel_data_type, pixel_subdata
	));
#ifdef GL_DEBUG_TEXTURES
	PTGN_LOG(
		"glTexSubImage2D(target=", target, ",mipmap_level=", mipmap_level,
		",offset=", subdata_offset, ",size=", subdata_size, ",pixel_format=", pixel_data_format,
		",pixel_type=", pixel_data_type, ",pixel_subdata=", pixel_subdata, ")"
	);
#endif
}

void Textures::SetTextureClampBorderColor(TextureId texture, Color color) const {
	PTGN_ASSERT(
		gl_.IsBound(texture), "TextureId must be bound prior to setting its clamp border color"
	);

	auto c{ static_cast<V4_float>(color) };
	SetTextureParameter(texture, GL_TEXTURE_BORDER_COLOR, c.Data());
}

void Textures::SetTextureParameter(TextureId texture, GLenum param, const GLfloat* values) const {
	PTGN_ASSERT(gl_.IsBound(texture), "TextureId must be bound prior to setting its parameters");
	PTGN_ASSERT(values != nullptr, "Cannot set texture parameter values to nullptr");
	constexpr AttachmentObject target{ AttachmentObject::Texture2D };
	GLCall(glTexParameterfv(std::to_underlying(target), param, values));
#ifdef GL_DEBUG_TEXTURES
	PTGN_LOG("glTexParameterfv(target=", target, ",param=", param, ",values=", values, ")");
#endif
}

void Textures::SetTextureParameter(TextureId texture, GLenum param, const GLint* values) const {
	PTGN_ASSERT(gl_.IsBound(texture), "TextureId must be bound prior to setting its parameters");
	PTGN_ASSERT(values != nullptr, "Cannot set texture parameter values to nullptr");
	constexpr AttachmentObject target{ AttachmentObject::Texture2D };
	GLCall(glTexParameteriv(std::to_underlying(target), param, values));
#ifdef GL_DEBUG_TEXTURES
	PTGN_LOG("glTexParameteriv(target=", target, ",param=", param, ",values=", values, ")");
#endif
}

void Textures::SetTextureParameter(TextureId texture, GLenum param, GLfloat value) const {
	PTGN_ASSERT(gl_.IsBound(texture), "TextureId must be bound prior to setting its parameters");
	PTGN_ASSERT(value != -1, "Cannot set texture parameter value to -1");
	constexpr AttachmentObject target{ AttachmentObject::Texture2D };
	GLCall(glTexParameterf(std::to_underlying(target), param, value));
#ifdef GL_DEBUG_TEXTURES
	PTGN_LOG("glTexParameterf(target=", target, ",param=", param, ",value=", value, ")");
#endif
}

void Textures::SetTextureParameter(TextureId texture, GLenum param, GLint value) const {
	PTGN_ASSERT(gl_.IsBound(texture), "TextureId must be bound prior to setting its parameters");
	PTGN_ASSERT(value != -1, "Cannot set texture parameter value to -1");
	constexpr AttachmentObject target{ AttachmentObject::Texture2D };
	GLCall(glTexParameteri(std::to_underlying(target), param, value));
#ifdef GL_DEBUG_TEXTURES
	PTGN_LOG("glTexParameteri(target=", target, ",param=", param, ",value=", value, ")");
#endif
}

GLint Textures::GetTextureParameter(TextureId texture, GLenum param) const {
	PTGN_ASSERT(gl_.IsBound(texture), "TextureId must be bound prior to getting its parameters");
	GLint value{ -1 };
	constexpr AttachmentObject target{ AttachmentObject::Texture2D };
	GLCall(glGetTexParameteriv(std::to_underlying(target), param, &value));
#ifdef GL_DEBUG_TEXTURES
	PTGN_LOG("glGetTexParameteriv(target=", target, ",param=", param, ") -> value=", value);
#endif
	PTGN_ASSERT(value != -1, "Failed to retrieve texture parameter");
	return value;
}

bool Textures::SupportsMipmaps(GLenum texture_min_filter) {
	return texture_min_filter == GL_LINEAR_MIPMAP_LINEAR ||
		   texture_min_filter == GL_LINEAR_MIPMAP_NEAREST ||
		   texture_min_filter == GL_NEAREST_MIPMAP_LINEAR ||
		   texture_min_filter == GL_NEAREST_MIPMAP_NEAREST;
}

void Textures::GenerateMipmaps(TextureId texture) const {
	PTGN_ASSERT(gl_.IsBound(texture), "TextureId must be bound prior to generating mipmaps for it");
#ifndef __EMSCRIPTEN__
	PTGN_ASSERT(
		SupportsMipmaps(GetTextureParameter(texture, GL_TEXTURE_MIN_FILTER)),
		"Set texture minifying scaling to mipmap type before generating mipmaps"
	);
#endif
	constexpr AttachmentObject target{ AttachmentObject::Texture2D };
	GLCall(GenerateMipmap(std::to_underlying(target)));
#ifdef GL_DEBUG_TEXTURES
	PTGN_LOG("glGenerateMipmap(target=", target, ")");
#endif
}

TextureId Textures::CreateTexture() {
	TextureId id{ 0 };
	GLCall(glGenTextures(1, &id.value));
#ifdef GL_DEBUG_TEXTURES
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
#ifdef GL_DEBUG_TEXTURES
	PTGN_LOG("glDeleteTextures(id=", id.value, ")");
#endif
	cache_.Remove(id);
}

} // namespace ptgn::impl::gl