#include "renderer/backend/gl/gl_texture.h"

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "core/math/vector4.h"
#include "core/util/id_map.h"
#include "renderer/backend/gl/gl.h"
#include "renderer/backend/gl/gl_context.h"
#include "renderer/resources/texture.h"

namespace ptgn::impl::gl {

Textures::Textures(GLContext& gl) : gl_{ gl } {}

TextureFormat TextureCache::GetFormat() const {
	return GetTextureFormatFromInternal(internal_format);
}

Texture Textures::CreateTexture(
	const void* pixel_data, GLenum pixel_data_format, GLenum pixel_data_type, V2_int size,
	GLenum internal_format, bool restore_bind
) {
	auto texture{ CreateTexture() };

	auto _ = gl_.Bind(texture, restore_bind);

	SetTextureData(texture, pixel_data, pixel_data_format, pixel_data_type, size, internal_format);

	SetTextureParameter(texture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	SetTextureParameter(texture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	SetTextureParameter(texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	SetTextureParameter(texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	return texture;
}

V2_int Textures::GetTextureSize(Texture texture) const {
	PTGN_ASSERT(cache_.Has(texture), "Texture not in cache");
	return cache_.Get(texture).size;
}

void Textures::ResizeTexture(Texture texture, V2_int new_size) {
	PTGN_ASSERT(texture);

	const auto& cache = cache_.Get(texture);

	if (cache.size == new_size) {
		return;
	}

	auto _ = gl_.Bind(texture, true);

	SetTextureData(texture, nullptr, GL_RGBA, GL_UNSIGNED_BYTE, new_size, cache.internal_format);
}

TextureCache& Textures::GetCache(Texture texture) {
	PTGN_ASSERT(cache_.Has(texture), "No texture with id ", texture, " in cache");
	return cache_.Get(texture);
}

const TextureCache& Textures::GetCache(Texture texture) const {
	PTGN_ASSERT(cache_.Has(texture), "No texture with id ", texture, " in cache");
	return cache_.Get(texture);
}

void Textures::SetTextureData(
	Texture texture, const void* pixel_data, GLenum pixel_data_format, GLenum pixel_data_type,
	V2_int size, GLenum internal_format
) {
	PTGN_ASSERT(gl_.IsBound(texture), "Texture must be bound prior to setting its data");

	constexpr GLint mipmap_level{ 0 };
	constexpr GLint border{ 0 };

#ifdef __EMSCRIPTEN__
	PTGN_ASSERT(
		pixel_data_format != GL_BGRA && pixel_data_format != GL_BGR && internal_format != GL_BGRA &&
			internal_format != GL_BGR,
		"OpenGL ES3.0 does not support BGR(A) formats in glTexImage2D"
	);
#endif

	GLCall(glTexImage2D(
		GL_TEXTURE_2D, mipmap_level, internal_format, size.x, size.y, border, pixel_data_format,
		pixel_data_type, pixel_data
	));

	auto& cache			  = cache_.Get(texture);
	cache.size			  = size;
	cache.internal_format = internal_format;
}

void Textures::SetTextureSubData(
	Texture texture, const void* pixel_subdata, GLenum pixel_data_format, GLenum pixel_data_type,
	V2_int subdata_size, V2_int subdata_offset
) const {
	PTGN_ASSERT(gl_.IsBound(texture), "Texture must be bound prior to setting its subdata");
	PTGN_ASSERT(pixel_subdata != nullptr, "Cannot set texture subdata to nullptr");

	constexpr GLint mipmap_level{ 0 };

	GLCall(glTexSubImage2D(
		GL_TEXTURE_2D, mipmap_level, subdata_offset.x, subdata_offset.y, subdata_size.x,
		subdata_size.y, pixel_data_format, pixel_data_type, pixel_subdata
	));
}

void Textures::SetTextureClampBorderColor(Texture texture, Color color) const {
	PTGN_ASSERT(
		gl_.IsBound(texture), "Texture must be bound prior to setting its clamp border color"
	);

	auto c{ static_cast<V4_float>(color) };
	SetTextureParameter(texture, GL_TEXTURE_BORDER_COLOR, c.Data());
}

void Textures::SetTextureParameter(Texture texture, GLenum param, const GLfloat* values) const {
	PTGN_ASSERT(gl_.IsBound(texture), "Texture must be bound prior to setting its parameters");
	PTGN_ASSERT(values != nullptr, "Cannot set texture parameter values to nullptr");
	GLCall(glTexParameterfv(GL_TEXTURE_2D, param, values));
}

void Textures::SetTextureParameter(Texture texture, GLenum param, const GLint* values) const {
	PTGN_ASSERT(gl_.IsBound(texture), "Texture must be bound prior to setting its parameters");
	PTGN_ASSERT(values != nullptr, "Cannot set texture parameter values to nullptr");
	GLCall(glTexParameteriv(GL_TEXTURE_2D, param, values));
}

void Textures::SetTextureParameter(Texture texture, GLenum param, GLfloat value) const {
	PTGN_ASSERT(gl_.IsBound(texture), "Texture must be bound prior to setting its parameters");
	PTGN_ASSERT(value != -1, "Cannot set texture parameter value to -1");
	GLCall(glTexParameterf(GL_TEXTURE_2D, param, value));
}

void Textures::SetTextureParameter(Texture texture, GLenum param, GLint value) const {
	PTGN_ASSERT(gl_.IsBound(texture), "Texture must be bound prior to setting its parameters");
	PTGN_ASSERT(value != -1, "Cannot set texture parameter value to -1");
	GLCall(glTexParameteri(GL_TEXTURE_2D, param, value));
}

GLint Textures::GetTextureParameter(Texture texture, GLenum param) const {
	PTGN_ASSERT(gl_.IsBound(texture), "Texture must be bound prior to getting its parameters");
	GLint value{ -1 };
	GLCall(glGetTexParameteriv(GL_TEXTURE_2D, param, &value));
	PTGN_ASSERT(value != -1, "Failed to retrieve texture parameter");
	return value;
}

bool Textures::SupportsMipmaps(GLenum texture_min_filter) {
	return texture_min_filter == GL_LINEAR_MIPMAP_LINEAR ||
		   texture_min_filter == GL_LINEAR_MIPMAP_NEAREST ||
		   texture_min_filter == GL_NEAREST_MIPMAP_LINEAR ||
		   texture_min_filter == GL_NEAREST_MIPMAP_NEAREST;
}

void Textures::GenerateMipmaps(Texture texture) const {
	PTGN_ASSERT(gl_.IsBound(texture), "Texture must be bound prior to generating mipmaps for it");
#ifndef __EMSCRIPTEN__
	PTGN_ASSERT(
		SupportsMipmaps(GetTextureParameter(texture, GL_TEXTURE_MIN_FILTER)),
		"Set texture minifying scaling to mipmap type before generating mipmaps"
	);
#endif
	GLCall(GenerateMipmap(GL_TEXTURE_2D));
}

Texture Textures::CreateTexture() {
	Texture id{ 0 };
	GLCall(glGenTextures(1, &id.value));
	PTGN_ASSERT(id, "Failed to create texture");
	cache_.Add(id, TextureCache{});
	return id;
}

void Textures::DestroyTexture(Texture id) {
	if (!id) {
		return;
	}
	GLCall(glDeleteTextures(1, &id.value));
	cache_.Remove(id);
}

} // namespace ptgn::impl::gl