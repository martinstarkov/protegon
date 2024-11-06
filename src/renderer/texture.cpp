#include "renderer/texture.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <type_traits>
#include <vector>

#include "core/game.h"
#include "math/vector2.h"
#include "math/vector4.h"
#include "renderer/color.h"
#include "renderer/gl_helper.h"
#include "renderer/gl_loader.h"
#include "renderer/gl_renderer.h"
#include "renderer/surface.h"
#include "utility/debug.h"
#include "utility/file.h"
#include "utility/handle.h"
#include "utility/log.h"

namespace ptgn {

namespace impl {

#define PUSHSTATE()           \
	std::int32_t restore_id { \
		Texture::GetBoundId() \
	}
#define POPSTATE() GLCall(gl::glBindTexture(GL_TEXTURE_2D, restore_id))

TextureInstance::TextureInstance() {
	GLCall(gl::glGenTextures(1, &id_));
	PTGN_ASSERT(id_ != 0, "Failed to generate texture using OpenGL context");
}

TextureInstance::~TextureInstance() {
	GLCall(gl::glDeleteTextures(1, &id_));
}

GLFormats GetGLFormats(ImageFormat format) {
	// Possible internal format options:
	// GL_R#size, GL_RG#size, GL_RGB#size, GL_RGBA#size
	switch (format) {
		case ImageFormat::RGBA8888: {
			return { InternalGLFormat::RGBA8, GL_RGBA, 4 };
		}
		case ImageFormat::RGB888: {
			return { InternalGLFormat::RGB8, GL_RGB, 3 };
		}
#ifdef __EMSCRIPTEN__
		case ImageFormat::BGRA8888:
		case ImageFormat::BGR888:	{
			PTGN_ERROR("OpenGL ES3.0 does not support BGR(A) texture formats in glTexImage2D");
		}
#else
		case ImageFormat::BGRA8888: {
			return { InternalGLFormat::RGBA8, GL_BGRA, 4 };
		}
		case ImageFormat::BGR888: {
			return { InternalGLFormat::RGB8, GL_BGR, 3 };
		}
#endif
		default: break;
	}
	PTGN_ERROR("Could not determine OpenGL formats for given ImageFormat");
}

} // namespace impl

Texture::Texture(const path& image_path, ImageFormat format) :
	Texture{
		std::invoke([&]() {
			PTGN_ASSERT(
				format != ImageFormat::Unknown, "Cannot create texture with unknown image format"
			);
			PTGN_ASSERT(
				FileExists(image_path),
				"Cannot create texture from file path which does not exist: ", image_path.string()
			);
			return Surface{ image_path };
		}),
	} {}

Texture::Texture(const Surface& surface) : Texture{ surface.GetData(), surface.GetSize() } {}

Texture::Texture(
	const void* pixel_data, const V2_int& size, ImageFormat format, TextureWrapping wrapping,
	TextureFilter minifying, TextureFilter magnifying, bool mipmaps
) {
	PUSHSTATE();

	Create();

	Bind();

	SetDataImpl(pixel_data, size, format);

	GLCall(gl::glTexParameteri(
		GL_TEXTURE_2D, static_cast<gl::GLenum>(impl::TextureParameter::WrapS),
		static_cast<int>(wrapping)
	));
	GLCall(gl::glTexParameteri(
		GL_TEXTURE_2D, static_cast<gl::GLenum>(impl::TextureParameter::WrapT),
		static_cast<int>(wrapping)
	));
	GLCall(gl::glTexParameteri(
		GL_TEXTURE_2D, static_cast<gl::GLenum>(impl::TextureParameter::MinFilter),
		static_cast<int>(minifying)
	));
	GLCall(gl::glTexParameteri(
		GL_TEXTURE_2D, static_cast<gl::GLenum>(impl::TextureParameter::MagFilter),
		static_cast<int>(magnifying)
	));

	mipmaps =
		mipmaps && (minifying != TextureFilter::Linear && minifying != TextureFilter::Nearest ||
					magnifying != TextureFilter::Linear && magnifying != TextureFilter::Nearest);

	if (mipmaps) {
		GLCall(gl::GenerateMipmap(GL_TEXTURE_2D));
	}

	POPSTATE();
}

Texture::Texture(const std::vector<Color>& pixels, const V2_int& size) :
	Texture{ std::invoke([&]() {
				 PTGN_ASSERT(
					 pixels.size() == size.x * size.y,
					 "Provided pixel array must match texture size"
				 );
				 return (void*)pixels.data();
			 }),
			 size, ImageFormat::RGBA8888 } {}

void Texture::Bind() const {
	GLCall(gl::glBindTexture(GL_TEXTURE_2D, Get().id_));
#ifdef PTGN_DEBUG
	++game.stats.texture_binds;
#endif
}

void Texture::Bind(std::uint32_t slot) const {
	SetActiveSlot(slot);
	Bind();
}

void Texture::SetActiveSlot(std::uint32_t slot) const {
	PTGN_ASSERT(
		static_cast<std::int32_t>(slot) < GLRenderer::GetMaxTextureSlots(),
		"Attempting to bind a slot outside of OpenGL texture slot maximum"
	);
	GLCall(gl::ActiveTexture(GL_TEXTURE0 + slot));
}

std::int32_t Texture::GetBoundId() {
	std::int32_t id{ -1 };
	GLCall(gl::glGetIntegerv(static_cast<gl::GLenum>(impl::GLBinding::Texture2D), &id));
	PTGN_ASSERT(id >= 0);
	return id;
}

std::int32_t Texture::GetActiveSlot() {
	std::int32_t id{ -1 };
	GLCall(gl::glGetIntegerv(GL_ACTIVE_TEXTURE, &id));
	PTGN_ASSERT(id >= 0);
	return id;
}

void Texture::SetDataImpl(const void* pixel_data, const V2_int& size, ImageFormat format) {
	PTGN_ASSERT(
		format != ImageFormat::Unknown, "Cannot set data of texture with unknown image format"
	);
	auto& t{ Get() };

	PTGN_ASSERT(GetBoundId() == static_cast<std::int32_t>(t.id_));

	t.size_ = size;

	auto formats = impl::GetGLFormats(format);

	GLCall(gl::glTexImage2D(
		GL_TEXTURE_2D, 0, static_cast<gl::GLint>(formats.internal_), t.size_.x, t.size_.y, 0,
		formats.format_, static_cast<gl::GLenum>(impl::GLType::UnsignedByte), pixel_data
	));
}

void Texture::SetSubData(const void* pixel_data, ImageFormat format) {
	PTGN_ASSERT(pixel_data != nullptr);
	const auto& t{ Get() };

	PUSHSTATE();

	auto formats = impl::GetGLFormats(format);

	Bind();

	GLCall(gl::glTexSubImage2D(
		GL_TEXTURE_2D, 0, 0, 0, t.size_.x, t.size_.y, formats.format_,
		static_cast<gl::GLenum>(impl::GLType::UnsignedByte), pixel_data
	));

	POPSTATE();
}

void Texture::SetSubData(const std::vector<Color>& pixels) {
	PTGN_ASSERT(
		pixels.size() == GetSize().x * GetSize().y, "Provided pixel array must match texture size"
	);
	SetSubData((void*)pixels.data(), ImageFormat::RGBA8888);
}

V2_int Texture::GetSize() const {
	return Get().size_;
}

void Texture::SetWrapping(TextureWrapping s) const {
	PUSHSTATE();

	Bind();

	GLCall(gl::glTexParameteri(
		GL_TEXTURE_2D, static_cast<gl::GLenum>(impl::TextureParameter::WrapS), static_cast<int>(s)
	));

	POPSTATE();
}

void Texture::SetWrapping(TextureWrapping s, TextureWrapping t) const {
	PUSHSTATE();

	Bind();

	GLCall(gl::glTexParameteri(
		GL_TEXTURE_2D, static_cast<gl::GLenum>(impl::TextureParameter::WrapS), static_cast<int>(s)
	));
	GLCall(gl::glTexParameteri(
		GL_TEXTURE_2D, static_cast<gl::GLenum>(impl::TextureParameter::WrapT), static_cast<int>(t)
	));

	POPSTATE();
}

void Texture::SetWrapping(TextureWrapping s, TextureWrapping t, TextureWrapping r) const {
	PUSHSTATE();

	Bind();

	GLCall(gl::glTexParameteri(
		GL_TEXTURE_2D, static_cast<gl::GLenum>(impl::TextureParameter::WrapS), static_cast<int>(s)
	));
	GLCall(gl::glTexParameteri(
		GL_TEXTURE_2D, static_cast<gl::GLenum>(impl::TextureParameter::WrapT), static_cast<int>(t)
	));
	GLCall(gl::glTexParameteri(
		GL_TEXTURE_2D, static_cast<gl::GLenum>(impl::TextureParameter::WrapR), static_cast<int>(r)
	));

	POPSTATE();
}

void Texture::SetFilters(TextureFilter minifying, TextureFilter magnifying) const {
	PUSHSTATE();

	Bind();

	GLCall(gl::glTexParameteri(
		GL_TEXTURE_2D, static_cast<gl::GLenum>(impl::TextureParameter::MinFilter),
		static_cast<int>(minifying)
	));
	GLCall(gl::glTexParameteri(
		GL_TEXTURE_2D, static_cast<gl::GLenum>(impl::TextureParameter::MagFilter),
		static_cast<int>(magnifying)
	));

	POPSTATE();
}

void Texture::SetClampBorderColor(const Color& color) const {
	PUSHSTATE();

	Bind();

	V4_float c{ color.Normalized() };
	std::array<float, 4> border_color{ c.x, c.y, c.z, c.w };

	GLCall(gl::glTexParameterfv(
		GL_TEXTURE_2D, static_cast<gl::GLenum>(impl::TextureParameter::BorderColor),
		border_color.data()
	));

	POPSTATE();
}

void Texture::GenerateMipmaps() const {
	PUSHSTATE();

	Bind();

	GLCall(gl::GenerateMipmap(GL_TEXTURE_2D));

	POPSTATE();
}

} // namespace ptgn