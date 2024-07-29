#include "protegon/texture.h"

#include "SDL.h"
#include "SDL_image.h"
#include "protegon/game.h"
#include "renderer/gl_helper.h"
#include "renderer/gl_loader.h"
#include "utility/debug.h"

namespace ptgn {

namespace impl {

#define PUSHSTATE()       \
	gl::GLint restore_id; \
	gl::glGetIntegerv(GL_TEXTURE_BINDING_2D, &restore_id)
#define POPSTATE() gl::glBindTexture(GL_TEXTURE_2D, restore_id)

TextureInstance::TextureInstance() {
	gl::glGenTextures(1, &id_);
}

TextureInstance::~TextureInstance() {
	gl::glDeleteTextures(1, &id_);
}

GLFormats GetGLFormats(ImageFormat format) {
	switch (format) {
		case ImageFormat::RGBA32:
		case ImageFormat::RGBA8888: {
			return { GL_RGBA8, GL_RGBA };
		}
		case ImageFormat::RGB888: {
			return { GL_RGB8, GL_RGB };
		}
		default: return {};
	}
}

} // namespace impl

Texture::Texture(const path& image_path, bool flip_vertically, ImageFormat format) :
	Texture{ [&]() -> Surface {
		PTGN_CHECK(
			FileExists(image_path), "Cannot create texture from file path which does not exist"
		);
		Surface surface{ image_path };
		if (flip_vertically) {
			surface.FlipVertically();
		}
		return surface;
	}() } {}

Texture::Texture(const Surface& surface) :
	Texture{ (void*)surface.GetData().data(), surface.GetSize(), surface.GetImageFormat() } {}

Texture::Texture(void* pixel_data, const V2_int& size, ImageFormat format) {
	PUSHSTATE();

	SetDataImpl(pixel_data, size, format);

	gl::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	gl::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	gl::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	gl::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	POPSTATE();
}

void Texture::Bind() const {
	PTGN_CHECK(IsValid(), "Cannot bind texture which is destroyed or uninitialized");
	gl::glBindTexture(GL_TEXTURE_2D, instance_->id_);
}

void Texture::Bind(std::uint32_t slot) const {
	std::int32_t max_texture_slots{ 0 };
	gl::glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_texture_slots);
	PTGN_ASSERT(max_texture_slots != 0);
	PTGN_CHECK(
		slot < static_cast<std::uint32_t>(max_texture_slots),
		"Attempting to bind a slot outside of OpenGL texture slot minimum"
	);
	gl::ActiveTexture(GL_TEXTURE0 + slot);
	Bind();
	// gl::BindTextureUnit(slot, instance_->id_);
}

void Texture::Unbind() const {
	gl::glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::SetData(void* pixel_data, const V2_int& size, ImageFormat format) {
	PUSHSTATE();

	SetDataImpl(pixel_data, size, format);

	POPSTATE();
}

void Texture::SetDataImpl(void* pixel_data, const V2_int& size, ImageFormat format) {
	if (!IsValid()) {
		instance_ = std::make_shared<impl::TextureInstance>();
	}

	instance_->size_ = size;

	Bind();

	auto formats = impl::GetGLFormats(format);

	gl::glTexImage2D(
		GL_TEXTURE_2D, 0, formats.internal_, instance_->size_.x, instance_->size_.y, 0,
		formats.format_, static_cast<gl::GLenum>(impl::GLType::UnsignedByte), pixel_data
	);
}

void Texture::SetSubData(void* pixel_data, ImageFormat format, const V2_int& offset) {
	PTGN_CHECK(IsValid(), "Cannot set subdata of uninitialized or destroyed texture instance");
	PTGN_ASSERT(offset.x < instance_->size_.x);
	PTGN_ASSERT(offset.y < instance_->size_.y);

	PUSHSTATE();

	auto formats = impl::GetGLFormats(format);

	Bind();

	gl::glTexSubImage2D(
		GL_TEXTURE_2D, 0, offset.x, offset.y, instance_->size_.x, instance_->size_.y,
		formats.format_, static_cast<gl::GLenum>(impl::GLType::UnsignedByte), pixel_data
	);

	POPSTATE();
}

V2_int Texture::GetSize() const {
	PTGN_CHECK(IsValid(), "Cannot get size of texture which is destroyed or uninitialized");
	return instance_->size_;
}

} // namespace ptgn