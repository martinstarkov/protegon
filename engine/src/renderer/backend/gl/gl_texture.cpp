#include "renderer/backend/gl/gl_texture.h"

#include <utility>

#include "core/assert.h"
#include "core/math/vector2.h"
#include "core/util/id_map.h"
#include "renderer/backend/gl/gl.h"
#include "renderer/backend/gl/gl_context.h"
#include "renderer/backend/gl/gl_framebuffer.h"
#include "renderer/resources/id.h"
#include "renderer/resources/texture_format.h"

namespace ptgn::impl::gl {

Textures::Textures(GLContext& gl) : gl_{ gl } {}

TextureId Textures::CreateTexture(V2_int size, TextureFormat format, TextureParameters params) {
	auto [pixel_format, pixel_type] = GetPixelDataFormat(format);
	return CreateTexture(nullptr, pixel_format, pixel_type, size, format, params);
}

TextureId Textures::CreateTexture(
	const void* pixel_data, PixelDataFormat pixel_data_format, PixelDataType pixel_data_type,
	V2_int size, TextureFormat format, TextureParameters params, bool restore_bind
) {
	auto texture{ CreateTexture() };

	auto _ = gl_.Bind(texture, restore_bind);

	SetTextureData(texture, pixel_data, pixel_data_format, pixel_data_type, size, format);

	using enum TextureParameter;

	SetTextureParameter(texture, MinFilter, std::to_underlying(params.min_filter));
	SetTextureParameter(texture, MagFilter, std::to_underlying(params.mag_filter));
	SetTextureParameter(texture, WrapS, std::to_underlying(params.wrap_s));
	SetTextureParameter(texture, WrapT, std::to_underlying(params.wrap_t));

	return texture;
}

V2_int Textures::GetTextureSize(TextureId texture) const {
	PTGN_ASSERT(cache_.Has(texture), "TextureId not in cache");
	return cache_.Get(texture).size;
}

TextureFormat Textures::GetTextureFormat(TextureId texture) const {
	PTGN_ASSERT(cache_.Has(texture), "TextureId not in cache");
	return cache_.Get(texture).format;
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

	auto& cache	 = cache_.Get(texture);
	cache.size	 = size;
	cache.format = format;
}

void Textures::SetTextureSubData(
	TextureId texture, const void* pixel_subdata, PixelDataFormat pixel_data_format,
	PixelDataType pixel_data_type, V2_int subdata_size, V2_int subdata_offset
) const {
	PTGN_ASSERT(gl_.IsBound(texture), "TextureId must be bound prior to setting its subdata");
	PTGN_ASSERT(pixel_subdata, "Cannot set texture subdata to nullptr");

	constexpr GLint mipmap_level{ 0 };
	constexpr AttachmentObject target{ AttachmentObject::Texture2D };

	GLCall(glTexSubImage2D(
		std::to_underlying(target), mipmap_level, subdata_offset.x, subdata_offset.y,
		subdata_size.x, subdata_size.y, std::to_underlying(pixel_data_format),
		std::to_underlying(pixel_data_type), pixel_subdata
	));
}

void Textures::SetTextureParameter(
	TextureId texture, TextureParameter param, const float* values
) const {
	PTGN_ASSERT(gl_.IsBound(texture), "TextureId must be bound prior to setting its parameters");
	PTGN_ASSERT(values, "Cannot set texture parameter values to nullptr");
	constexpr AttachmentObject target{ AttachmentObject::Texture2D };
	GLCall(glTexParameterfv(std::to_underlying(target), std::to_underlying(param), values));
}

void Textures::SetTextureParameter(
	TextureId texture, TextureParameter param, const int* values
) const {
	PTGN_ASSERT(gl_.IsBound(texture), "TextureId must be bound prior to setting its parameters");
	PTGN_ASSERT(values, "Cannot set texture parameter values to nullptr");
	constexpr AttachmentObject target{ AttachmentObject::Texture2D };
	GLCall(glTexParameteriv(std::to_underlying(target), std::to_underlying(param), values));
}

void Textures::SetTextureParameter(TextureId texture, TextureParameter param, float value) const {
	PTGN_ASSERT(gl_.IsBound(texture), "TextureId must be bound prior to setting its parameters");
	PTGN_ASSERT(value != -1, "Cannot set texture parameter value to -1");
	constexpr AttachmentObject target{ AttachmentObject::Texture2D };
	GLCall(glTexParameterf(std::to_underlying(target), std::to_underlying(param), value));
}

void Textures::SetTextureParameter(TextureId texture, TextureParameter param, int value) const {
	PTGN_ASSERT(gl_.IsBound(texture), "TextureId must be bound prior to setting its parameters");
	PTGN_ASSERT(value != -1, "Cannot set texture parameter value to -1");
	constexpr AttachmentObject target{ AttachmentObject::Texture2D };
	GLCall(glTexParameteri(std::to_underlying(target), std::to_underlying(param), value));
}

int Textures::GetTextureParameter(TextureId texture, TextureParameter param) const {
	PTGN_ASSERT(gl_.IsBound(texture), "TextureId must be bound prior to getting its parameters");
	GLint value{ -1 };
	constexpr AttachmentObject target{ AttachmentObject::Texture2D };
	GLCall(glGetTexParameteriv(std::to_underlying(target), std::to_underlying(param), &value));
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
	GLCall(glGenerateMipmap(std::to_underlying(target)));
}

TextureId Textures::CreateTexture() {
	TextureId id{ 0 };
	GLCall(glGenTextures(1, &id.value));
	PTGN_ASSERT(id, "Failed to create texture");
	cache_.Add(id, TextureCache{});
	return id;
}

void Textures::DestroyTexture(TextureId id) {
	if (!id) {
		return;
	}
	GLCall(glDeleteTextures(1, &id.value));
	cache_.Remove(id);
}

} // namespace ptgn::impl::gl