#include "renderer/backend/gl/gl_texture.h"

#include <optional>
#include <utility>

#include "core/assert.h"
#include "core/math/vector2.h"
#include "core/util/id_map.h"
#include "renderer/backend/gl/gl.h"
#include "renderer/backend/gl/gl_context.h"
#include "renderer/resources/id.h"
#include "renderer/resources/texture.h"
#include "renderer/resources/texture_format.h"

namespace ptgn::impl::gl {

Textures::Textures(GLContext& gl) : gl_{ gl } {}

TextureId Textures::Create(TextureDesc desc) {
	auto [pixel_format, pixel_type] = GetPixelDataFormat(desc.format);
	return Create(nullptr, pixel_format, pixel_type, desc);
}

TextureId Textures::Create(
	const void* pixel_data, PixelDataFormat pixel_data_format, PixelDataType pixel_data_type,
	TextureDesc desc, bool restore_bind
) {
	auto texture{ CreateImpl() };

	auto _ = gl_.Bind(texture, restore_bind);

	SetData(texture, pixel_data, pixel_data_format, pixel_data_type, desc.size, desc.format);

	using enum TextureParameter;

	SetParameter(texture, MinFilter, std::to_underlying(desc.params.min_filter));
	SetParameter(texture, MagFilter, std::to_underlying(desc.params.mag_filter));
	SetParameter(texture, WrapS, std::to_underlying(desc.params.wrap_s));
	SetParameter(texture, WrapT, std::to_underlying(desc.params.wrap_t));

	PTGN_ASSERT(cache_.Get(texture).desc.format == desc.format);
	PTGN_ASSERT(cache_.Get(texture).desc.size == desc.size);
	PTGN_ASSERT(cache_.Get(texture).desc.params.min_filter == desc.params.min_filter);
	PTGN_ASSERT(cache_.Get(texture).desc.params.mag_filter == desc.params.mag_filter);
	PTGN_ASSERT(cache_.Get(texture).desc.params.wrap_s == desc.params.wrap_s);
	PTGN_ASSERT(cache_.Get(texture).desc.params.wrap_t == desc.params.wrap_t);
	PTGN_ASSERT(GLCallReturn(glIsTexture(texture)), "Failed to create a valid OpenGL texture");

	return texture;
}

std::optional<TextureDesc> Textures::GetDesc(TextureId texture) const {
	if (!cache_.Has(texture)) {
		return std::nullopt;
	}
	return cache_.Get(texture).desc;
}

void Textures::Resize(TextureId texture, V2_int new_size) {
	PTGN_ASSERT(texture);

	const auto& cache = cache_.Get(texture);

	if (cache.desc.size == new_size) {
		return;
	}

	auto _ = gl_.Bind(texture, true);

	SetData(
		texture, nullptr, PixelDataFormat::RGBA, PixelDataType::UnsignedByte, new_size,
		cache.desc.format
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

void Textures::SetData(
	TextureId texture, const void* pixel_data, PixelDataFormat pixel_data_format,
	PixelDataType pixel_data_type, V2_int size, TextureFormat format
) {
	PTGN_ASSERT(gl_.IsBound(texture), "TextureId must be bound prior to setting its data");
	PTGN_ASSERT(size.IsPositive(), "Cannot create texture with zero size");

	constexpr int mipmap_level{ 0 };
	constexpr int border{ 0 };

	GLCall(glTexImage2D(
		GL_TEXTURE_2D, mipmap_level, std::to_underlying(format), size.x, size.y, border,
		std::to_underlying(pixel_data_format), std::to_underlying(pixel_data_type), pixel_data
	));

	auto& cache		  = cache_.Get(texture);
	cache.desc.size	  = size;
	cache.desc.format = format;
}

void Textures::SetSubData(
	TextureId texture, const void* pixel_subdata, PixelDataFormat pixel_data_format,
	PixelDataType pixel_data_type, V2_int subdata_size, V2_int subdata_offset
) const {
	PTGN_ASSERT(gl_.IsBound(texture), "TextureId must be bound prior to setting its subdata");
	PTGN_ASSERT(pixel_subdata, "Cannot set texture subdata to nullptr");

	constexpr GLint mipmap_level{ 0 };

	GLCall(glTexSubImage2D(
		GL_TEXTURE_2D, mipmap_level, subdata_offset.x, subdata_offset.y, subdata_size.x,
		subdata_size.y, std::to_underlying(pixel_data_format), std::to_underlying(pixel_data_type),
		pixel_subdata
	));
}

void Textures::SetParameter(TextureId texture, TextureParameter param, const float* values) const {
	PTGN_ASSERT(gl_.IsBound(texture), "TextureId must be bound prior to setting its parameters");
	PTGN_ASSERT(values, "Cannot set texture parameter values to nullptr");
	GLCall(glTexParameterfv(GL_TEXTURE_2D, std::to_underlying(param), values));
}

void Textures::SetParameter(TextureId texture, TextureParameter param, const int* values) const {
	PTGN_ASSERT(gl_.IsBound(texture), "TextureId must be bound prior to setting its parameters");
	PTGN_ASSERT(values, "Cannot set texture parameter values to nullptr");
	GLCall(glTexParameteriv(GL_TEXTURE_2D, std::to_underlying(param), values));
}

void Textures::SetParameter(TextureId texture, TextureParameter param, float value) const {
	PTGN_ASSERT(gl_.IsBound(texture), "TextureId must be bound prior to setting its parameters");
	PTGN_ASSERT(value != -1, "Cannot set texture parameter value to -1");
	GLCall(glTexParameterf(GL_TEXTURE_2D, std::to_underlying(param), value));
}

void Textures::SetParameter(TextureId texture, TextureParameter param, int value) {
	PTGN_ASSERT(gl_.IsBound(texture), "TextureId must be bound prior to setting its parameters");
	PTGN_ASSERT(value != -1, "Cannot set texture parameter value to -1");
	GLCall(glTexParameteri(GL_TEXTURE_2D, std::to_underlying(param), value));
	auto& cache{ cache_.Get(texture) };
	switch (param) {
		using enum TextureParameter;
		case MinFilter: cache.desc.params.min_filter = static_cast<TextureMinFilter>(value); break;
		case MagFilter: cache.desc.params.mag_filter = static_cast<TextureMagFilter>(value); break;
		case WrapS:		cache.desc.params.wrap_s = static_cast<TextureWrap>(value); break;
		case WrapT:		cache.desc.params.wrap_t = static_cast<TextureWrap>(value); break;
	}
}

int Textures::GetParameter(TextureId texture, TextureParameter param) const {
	PTGN_ASSERT(gl_.IsBound(texture), "TextureId must be bound prior to getting its parameters");
	GLint value{ -1 };
	GLCall(glGetTexParameteriv(GL_TEXTURE_2D, std::to_underlying(param), &value));
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
			static_cast<TextureMinFilter>(GetParameter(texture, TextureParameter::MinFilter))
		),
		"Set texture minifying scaling to mipmap type before generating mipmaps"
	);
#endif
	GLCall(glGenerateMipmap(GL_TEXTURE_2D));
}

TextureId Textures::CreateImpl() {
	TextureId id{ 0 };
	GLCall(glGenTextures(1, &id.value));
	PTGN_ASSERT(id, "Failed to create texture");
	cache_.Add(id, TextureCache{});
	return id;
}

void Textures::Destroy(TextureId id) {
	if (!id) {
		return;
	}
	gl_.ForgetId(id);
	PTGN_ASSERT(!gl_.IsBound(id), "TextureId must not be bound when destroying it");
	GLCall(glDeleteTextures(1, &id.value));
	cache_.Remove(id);
}

} // namespace ptgn::impl::gl