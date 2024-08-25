#include "protegon/texture.h"

#include "renderer/gl_helper.h"
#include "renderer/gl_loader.h"
#include "renderer/gl_renderer.h"
#include "utility/debug.h"

namespace ptgn {

namespace impl {

#define PUSHSTATE()           \
	std::int32_t restore_id { \
		Texture::GetBoundId() \
	}
#define POPSTATE() gl::glBindTexture(GL_TEXTURE_2D, restore_id)

TextureInstance::TextureInstance() {
	gl::glGenTextures(1, &id_);
	PTGN_ASSERT(id_ != 0, "Failed to generate texture using OpenGL context");
}

TextureInstance::~TextureInstance() {
	gl::glDeleteTextures(1, &id_);
}

static GLFormats GetGLFormats(ImageFormat format) {
	// Possible internal format options:
	// GL_R#size, GL_RG#size, GL_RGB#size, GL_RGBA#size
	switch (format) {
		case ImageFormat::RGBA8888: {
			return { InternalGLFormat::RGBA8, GL_RGBA };
		}
		case ImageFormat::RGB888: {
			return { InternalGLFormat::RGB8, GL_RGB };
		}
#ifdef __EMSCRIPTEN__
		case ImageFormat::BGRA8888:
		case ImageFormat::BGR888:	{
			PTGN_ERROR("OpenGL ES3.0 does not support BGR(A) texture formats in glTexImage2D");
		}
#else
		case ImageFormat::BGRA8888: {
			return { InternalGLFormat::RGBA8, GL_BGRA };
		}
		case ImageFormat::BGR888: {
			return { InternalGLFormat::RGB8, GL_BGR };
		}
#endif
		default: break;
	}
	PTGN_ERROR("Could not determine OpenGL formats for given ImageFormat");
}

} // namespace impl

Texture::Texture(const path& image_path, ImageFormat format) :
	Texture{
		[&]() -> Surface {
			PTGN_ASSERT(
				format != ImageFormat::Unknown, "Cannot create texture with unknown image format"
			);
			PTGN_ASSERT(
				FileExists(image_path),
				"Cannot create texture from file path which does not exist: ", image_path.string()
			);
			return Surface{ image_path };
		}(),
	} {}

Texture::Texture(const Surface& surface) : Texture{ surface.GetData(), surface.GetSize() } {}

Texture::Texture(const void* pixel_data, const V2_int& size, ImageFormat format) {
	PUSHSTATE();

	if (!IsValid()) {
		instance_ = std::make_shared<impl::TextureInstance>();
	}

	Bind();

	SetDataImpl(pixel_data, size, format);

	gl::glTexParameteri(
		GL_TEXTURE_2D, static_cast<gl::GLenum>(impl::TextureParameter::WrapS),
		static_cast<int>(default_wrapping)
	);
	gl::glTexParameteri(
		GL_TEXTURE_2D, static_cast<gl::GLenum>(impl::TextureParameter::WrapT),
		static_cast<int>(default_wrapping)
	);
	gl::glTexParameteri(
		GL_TEXTURE_2D, static_cast<gl::GLenum>(impl::TextureParameter::MinFilter),
		static_cast<int>(default_minifying_filter)
	);
	gl::glTexParameteri(
		GL_TEXTURE_2D, static_cast<gl::GLenum>(impl::TextureParameter::MagFilter),
		static_cast<int>(default_magnifying_filter)
	);

	if constexpr ((default_minifying_filter != TextureFilter::Linear && default_minifying_filter != TextureFilter::Nearest) || (default_magnifying_filter != TextureFilter::Linear && default_magnifying_filter != TextureFilter::Nearest)) {
		gl::GenerateMipmap(GL_TEXTURE_2D);
	}

	POPSTATE();
}

Texture::Texture(const std::vector<Color>& pixels, const V2_int& size) :
	Texture{ [&]() -> void* {
				PTGN_ASSERT(
					pixels.size() == size.x * size.y, "Provided pixel array must match texture size"
				);
				return (void*)pixels.data();
			}(),
			 size, ImageFormat::RGBA8888 } {}

void Texture::Bind() const {
	PTGN_ASSERT(IsValid(), "Cannot bind texture which is destroyed or uninitialized");
	gl::glBindTexture(GL_TEXTURE_2D, instance_->id_);
}

void Texture::Bind(std::uint32_t slot) const {
	PTGN_ASSERT(
		static_cast<std::int32_t>(slot) < GLRenderer::GetMaxTextureSlots(),
		"Attempting to bind a slot outside of OpenGL texture slot maximum"
	);
	gl::ActiveTexture(GL_TEXTURE0 + slot);
	Bind();
	// For newer versions of OpenGL:
	// gl::BindTextureUnit(slot, instance_->id_);
}

// void Texture::Unbind() {
//	gl::glBindTexture(GL_TEXTURE_2D, 0);
// }

std::int32_t Texture::GetBoundId() {
	std::int32_t id{ 0 };
	gl::glGetIntegerv(static_cast<gl::GLenum>(impl::GLBinding::Texture2D), &id);
	PTGN_ASSERT(id >= 0);
	return id;
}

void Texture::SetDataImpl(const void* pixel_data, const V2_int& size, ImageFormat format) {
	PTGN_ASSERT(
		format != ImageFormat::Unknown, "Cannot set data of texture with unknown image format"
	);
	PTGN_ASSERT(IsValid(), "Cannot set data of uninitialized or destroyed texture");
	PTGN_ASSERT(GetBoundId() == static_cast<std::int32_t>(instance_->id_));
	PTGN_ASSERT(pixel_data != nullptr);

	instance_->size_ = size;

	auto formats = impl::GetGLFormats(format);

	gl::glTexImage2D(
		GL_TEXTURE_2D, 0, static_cast<gl::GLint>(formats.internal_), instance_->size_.x,
		instance_->size_.y, 0, formats.format_, static_cast<gl::GLenum>(impl::GLType::UnsignedByte),
		pixel_data
	);
}

void Texture::SetSubData(const void* pixel_data, ImageFormat format) {
	PTGN_ASSERT(IsValid(), "Cannot set subdata of uninitialized or destroyed texture instance");
	PTGN_ASSERT(pixel_data != nullptr);

	PUSHSTATE();

	auto formats = impl::GetGLFormats(format);

	Bind();

	gl::glTexSubImage2D(
		GL_TEXTURE_2D, 0, 0, 0, instance_->size_.x, instance_->size_.y, formats.format_,
		static_cast<gl::GLenum>(impl::GLType::UnsignedByte), pixel_data
	);

	POPSTATE();
}

void Texture::SetSubData(const std::vector<Color>& pixels) {
	PTGN_ASSERT(
		pixels.size() == GetSize().x * GetSize().y, "Provided pixel array must match texture size"
	);
	SetSubData((void*)pixels.data(), ImageFormat::RGBA8888);
}

V2_int Texture::GetSize() const {
	PTGN_ASSERT(IsValid(), "Cannot get size of texture which is destroyed or uninitialized");
	return instance_->size_;
}

bool Texture::operator==(const Texture& o) const {
	return GetInstance() == o.GetInstance();
}

bool Texture::operator!=(const Texture& o) const {
	return !(*this == o);
}

void Texture::SetWrapping(TextureWrapping s) {
	PUSHSTATE();

	Bind();

	gl::glTexParameteri(
		GL_TEXTURE_2D, static_cast<gl::GLenum>(impl::TextureParameter::WrapS), static_cast<int>(s)
	);

	POPSTATE();
}

void Texture::SetWrapping(TextureWrapping s, TextureWrapping t) {
	PUSHSTATE();

	Bind();

	gl::glTexParameteri(
		GL_TEXTURE_2D, static_cast<gl::GLenum>(impl::TextureParameter::WrapS), static_cast<int>(s)
	);
	gl::glTexParameteri(
		GL_TEXTURE_2D, static_cast<gl::GLenum>(impl::TextureParameter::WrapT), static_cast<int>(t)
	);

	POPSTATE();
}

void Texture::SetWrapping(TextureWrapping s, TextureWrapping t, TextureWrapping r) {
	PUSHSTATE();

	Bind();

	gl::glTexParameteri(
		GL_TEXTURE_2D, static_cast<gl::GLenum>(impl::TextureParameter::WrapS), static_cast<int>(s)
	);
	gl::glTexParameteri(
		GL_TEXTURE_2D, static_cast<gl::GLenum>(impl::TextureParameter::WrapT), static_cast<int>(t)
	);
	gl::glTexParameteri(
		GL_TEXTURE_2D, static_cast<gl::GLenum>(impl::TextureParameter::WrapR), static_cast<int>(r)
	);

	POPSTATE();
}

void Texture::SetFilters(TextureFilter minifying, TextureFilter magnifying) {
	PUSHSTATE();

	Bind();

	gl::glTexParameteri(
		GL_TEXTURE_2D, static_cast<gl::GLenum>(impl::TextureParameter::MinFilter),
		static_cast<int>(minifying)
	);
	gl::glTexParameteri(
		GL_TEXTURE_2D, static_cast<gl::GLenum>(impl::TextureParameter::MagFilter),
		static_cast<int>(magnifying)
	);

	POPSTATE();
}

void Texture::SetClampBorderColor(const Color& color) {
	PUSHSTATE();

	Bind();

	V4_float c{ color.Normalized() };
	float border_color[4]{ c.x, c.y, c.z, c.w };

	gl::glTexParameterfv(
		GL_TEXTURE_2D, static_cast<gl::GLenum>(impl::TextureParameter::BorderColor), border_color
	);

	POPSTATE();
}

void Texture::GenerateMipmaps() {
	PUSHSTATE();

	Bind();

	gl::GenerateMipmap(GL_TEXTURE_2D);

	POPSTATE();
}

} // namespace ptgn