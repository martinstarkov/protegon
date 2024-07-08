#include "protegon/texture.h"

#include "SDL.h"
#include "SDL_image.h"

#include "core/game.h"
#include "gl_helper.h"
#include "protegon/debug.h"
#include "protegon/renderer.h"
#include "renderer/gl_loader.h"

namespace ptgn {

/*

void Texture::Draw(
	const Rectangle<float>& destination, const Rectangle<int>& source, float angle, Flip flip,
	V2_int* center_of_rotation
) const {
	renderer::DrawTexture(*this, destination, source, angle, flip, center_of_rotation);
}

V2_int Texture::GetSize() const {
	PTGN_CHECK(IsValid(), "Cannot get size of uninitialized or destroyed texture");
	V2_int size;
	SDL_QueryTexture(instance_.get(), NULL, NULL, &size.x, &size.y);
	return size;
}

void Texture::SetBlendMode(BlendMode mode) {
	PTGN_CHECK(IsValid(), "Cannot set blend mode of uninitialized or destroyed texture");
	SDL_SetTextureBlendMode(instance_.get(), static_cast<SDL_BlendMode>(mode));
}

void Texture::SetAlpha(std::uint8_t alpha) {
	PTGN_CHECK(IsValid(), "Cannot set alpha of uninitialized or destroyed texture");
	SDL_SetTextureBlendMode(instance_.get(), static_cast<SDL_BlendMode>(BlendMode::Blend));
	SDL_SetTextureAlphaMod(instance_.get(), alpha);
}

void Texture::SetColor(const Color& color) {
	PTGN_CHECK(IsValid(), "Cannot set color of uninitialized or destroyed texture");
	SetAlpha(color.a);
	SDL_SetTextureColorMod(instance_.get(), color.r, color.g, color.b);
}

*/

namespace impl {

TextureInstance::TextureInstance(const Surface& surface) {
	PTGN_ASSERT(surface.IsValid());
	size_ = surface.GetSize();
	
	GLFormats formats{ GetGLFormats(surface.GetImageFormat()) };

	GLSLType type{ GetType<std::uint8_t>() };
	
	gl::glGenTextures(1, &id_);
	gl::glBindTexture(GL_TEXTURE_2D, id_);
	
	gl::glTexImage2D(
		GL_TEXTURE_2D, 0, formats.internal_, size_.x, size_.y, 0, formats.format_,
		static_cast<gl::GLenum>(type),
		surface.GetData().data()
	);

	// Linear resampling when scaling up and down.
	gl::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	gl::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	gl::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); // or GL_CLAMP_TO_EDGE
	gl::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT); // or GL_CLAMP_TO_EDGE

	gl::glBindTexture(GL_TEXTURE_2D, 0);
}

TextureInstance::~TextureInstance() {
	gl::glDeleteTextures(1, &id_);
}

TextureInstance::GLFormats TextureInstance::GetGLFormats(ImageFormat format) {
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

Texture::Texture(const Surface& surface) {
	instance_ = std::shared_ptr<impl::TextureInstance>(new impl::TextureInstance(surface));
}

void Texture::Bind(std::uint32_t slot) const {
	std::int32_t max_texture_slots{ 0 };
	gl::glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_texture_slots);
	PTGN_ASSERT(max_texture_slots != 0);
	PTGN_CHECK(
		slot < max_texture_slots, "Attempting to bind a slot outside of OpenGL texture slot minimum"
	);
	PTGN_CHECK(IsValid(), "Cannot bind texture which is destroyed or uninitialized");
	gl::ActiveTexture(GL_TEXTURE0 + slot);
	gl::glBindTexture(GL_TEXTURE_2D, instance_->id_);
	//gl::BindTextureUnit(slot, instance_->id_);
}

void Texture::Unbind() const {
	gl::glBindTexture(GL_TEXTURE_2D, 0);
}

V2_int Texture::GetSize() const {
	PTGN_CHECK(IsValid(), "Cannot get size of texture which is destroyed or uninitialized");
	return instance_->size_;
}

} // namespace ptgn