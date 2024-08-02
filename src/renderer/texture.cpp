#include "protegon/texture.h"

#include "SDL.h"
#include "SDL_image.h"
#include "protegon/game.h"
#include "renderer/gl_helper.h"
#include "renderer/gl_loader.h"
#include "utility/debug.h"

namespace ptgn {

namespace impl {

#define PUSHSTATE()           \
	std::int32_t restore_id { \
		Texture::BoundId()    \
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
			return { GL_RGBA8, GL_RGBA };
		}
		case ImageFormat::RGB888: {
			return { GL_RGB8, GL_RGB };
		}
		case ImageFormat::BGRA8888: {
			return { GL_RGBA8, GL_BGRA };
		}
		case ImageFormat::BGR888: {
			return { GL_RGB8, GL_BGR };
		}
		default: break;
	}
	PTGN_ERROR("Could not determine OpenGL formats for given ImageFormat");
}

} // namespace impl

Texture::Texture(const path& image_path, ImageFormat format) :
	Texture{ [&]() -> Surface {
		PTGN_ASSERT(
			format != ImageFormat::Unknown, "Cannot create texture with unknown image format"
		);
		PTGN_ASSERT(
			FileExists(image_path), "Cannot create texture from file path which does not exist"
		);
		return Surface{ image_path };
	}() } {}

Texture::Texture(const Surface& surface) : Texture{ surface.GetData(), surface.GetSize() } {}

Texture::Texture(const void* pixel_data, const V2_int& size, ImageFormat format) {
	PUSHSTATE();

	if (!IsValid()) {
		instance_ = std::make_shared<impl::TextureInstance>();
	}

	Bind();

	SetDataImpl(pixel_data, size, format);

	gl::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	gl::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	gl::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	gl::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

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
		[&]() -> bool {
			std::int32_t max_texture_slots{ 0 };
			gl::glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_texture_slots);
			PTGN_ASSERT(max_texture_slots != 0);
			return slot < static_cast<std::uint32_t>(max_texture_slots);
		}(),
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

std::int32_t Texture::BoundId() {
	std::int32_t id{ 0 };
	gl::glGetIntegerv(GL_TEXTURE_BINDING_2D, &id);
	PTGN_ASSERT(id >= 0);
	return id;
}

void Texture::SetDataImpl(const void* pixel_data, const V2_int& size, ImageFormat format) {
	PTGN_ASSERT(
		format != ImageFormat::Unknown, "Cannot set data of texture with unknown image format"
	);
	PTGN_ASSERT(IsValid(), "Cannot set data of uninitialized or destroyed texture");
	PTGN_ASSERT(static_cast<std::uint32_t>(BoundId()) == instance_->id_);
	PTGN_ASSERT(pixel_data != nullptr);

	instance_->size_ = size;

	auto formats = impl::GetGLFormats(format);

	gl::glTexImage2D(
		GL_TEXTURE_2D, 0, formats.internal_, instance_->size_.x, instance_->size_.y, 0,
		formats.format_, static_cast<gl::GLenum>(impl::GLType::UnsignedByte), pixel_data
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

} // namespace ptgn