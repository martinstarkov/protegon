#include "protegon/texture.h"

#include <SDL.h>
#include <SDL_image.h>

#include "core/game.h"
#include "gl_helper.h"
#include "protegon/debug.h"
#include "protegon/renderer.h"
#include "renderer/gl_loader.h"

namespace ptgn {

// namespace Utils {
//
//		static GLenum HazelImageFormatToGLDataFormat(ImageFormat format)
//		{
//			switch (format)
//			{
//				case ImageFormat::RGB8:  return GL_RGB;
//				case ImageFormat::RGBA8: return GL_RGBA;
//			}
//
//			HZ_CORE_ASSERT(false);
//			return 0;
//		}
//
//		static GLenum HazelImageFormatToGLInternalFormat(ImageFormat format)
//		{
//			switch (format)
//			{
//			case ImageFormat::RGB8:  return GL_RGB8;
//			case ImageFormat::RGBA8: return GL_RGBA8;
//			}
//
//			HZ_CORE_ASSERT(false);
//			return 0;
//		}
//
//	}
//
//	OpenGLTexture2D::OpenGLTexture2D(const TextureSpecification& specification)
//		: m_Specification(specification), m_Width(m_Specification.Width),
// m_Height(m_Specification.Height)
//	{
//		HZ_PROFILE_FUNCTION();
//
//		m_InternalFormat = Utils::HazelImageFormatToGLInternalFormat(m_Specification.Format);
//		m_DataFormat = Utils::HazelImageFormatToGLDataFormat(m_Specification.Format);
//
//		glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
//		glTextureStorage2D(m_RendererID, 1, m_InternalFormat, m_Width, m_Height);
//
//		glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
//		glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
//
//		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
//		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);
//	}
//
//	OpenGLTexture2D::OpenGLTexture2D(const std::string& path)
//		: m_Path(path)
//	{
//		HZ_PROFILE_FUNCTION();
//
//		int width, height, channels;
//		stbi_set_flip_vertically_on_load(1);
//		stbi_uc* data = nullptr;
//		{
//			HZ_PROFILE_SCOPE("stbi_load - OpenGLTexture2D::OpenGLTexture2D(const std::string&)");
//			data = stbi_load(path.c_str(), &width, &height, &channels, 0);
//		}
//
//		if (data)
//		{
//			m_IsLoaded = true;
//
//			m_Width = width;
//			m_Height = height;
//
//			GLenum internalFormat = 0, dataFormat = 0;
//			if (channels == 4)
//			{
//				internalFormat = GL_RGBA8;
//				dataFormat = GL_RGBA;
//			}
//			else if (channels == 3)
//			{
//				internalFormat = GL_RGB8;
//				dataFormat = GL_RGB;
//			}
//
//			m_InternalFormat = internalFormat;
//			m_DataFormat = dataFormat;
//
//			HZ_CORE_ASSERT(internalFormat & dataFormat, "Format not supported!");
//
//			glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
//			glTextureStorage2D(m_RendererID, 1, internalFormat, m_Width, m_Height);
//
//			glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
//			glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
//
//			glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
//			glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);
//
//			glTextureSubImage2D(m_RendererID, 0, 0, 0, m_Width, m_Height, dataFormat,
// GL_UNSIGNED_BYTE, data);
//
//			stbi_image_free(data);
//		}
//	}
//
//	OpenGLTexture2D::~OpenGLTexture2D()
//	{
//		HZ_PROFILE_FUNCTION();
//
//		glDeleteTextures(1, &m_RendererID);
//	}
//
//	void OpenGLTexture2D::SetData(void* data, uint32_t size)
//	{
//		HZ_PROFILE_FUNCTION();
//
//		uint32_t bpp = m_DataFormat == GL_RGBA ? 4 : 3;
//		HZ_CORE_ASSERT(size == m_Width * m_Height * bpp, "Data must be entire texture!");
//		glTextureSubImage2D(m_RendererID, 0, 0, 0, m_Width, m_Height, m_DataFormat,
// GL_UNSIGNED_BYTE, data);
//	}
//
//	void OpenGLTexture2D::Bind(uint32_t slot) const
//	{
//		HZ_PROFILE_FUNCTION();
//
//		glBindTextureUnit(slot, m_RendererID);
//	}

/*
Texture::Texture(const path& image_path) {
	PTGN_CHECK(FileExists(image_path), "Cannot create texture from a nonexistent image path");
	instance_ = {
		IMG_LoadTexture(global::GetGame().sdl.GetRenderer().get(), image_path.string().c_str()),
		SDL_DestroyTexture
	};
	if (!IsValid()) {
		PTGN_ERROR(IMG_GetError());
		PTGN_ASSERT(false, "Failed to create texture from image path");
	}
}

Texture::Texture(AccessType access, const V2_int& size) {
	instance_ = { SDL_CreateTexture(
					  global::GetGame().sdl.GetRenderer().get(), SDL_PIXELFORMAT_RGBA8888,
					  static_cast<SDL_TextureAccess>(access), size.x, size.y
				  ),
				  SDL_DestroyTexture };
	PTGN_ASSERT(IsValid(), "Failed to create texture from access type and size");
}

Texture::Texture(const std::shared_ptr<SDL_Surface>& surface) {
	PTGN_ASSERT(
		surface != nullptr, "Cannot create texture from uninitialized or destroyed surface"
	);
	if (!IsValid()) {
		PTGN_ERROR(SDL_GetError());
		PTGN_ASSERT(false, "Failed to create texture from surface");
	}
}

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

Texture::AccessType Texture::GetAccessType() const {
	PTGN_CHECK(IsValid(), "Cannot get access type of uninitialized or destroyed texture");
	int access;
	SDL_QueryTexture(instance_.get(), nullptr, &access, NULL, NULL);
	return static_cast<Texture::AccessType>(access);
}

*/

namespace impl {

TextureInstance::TextureInstance(const Surface& surface) {
	PTGN_ASSERT(surface.IsValid());
	size_ = surface.GetSize();

	glGenTextures(1, &id_);
	glBindTexture(GL_TEXTURE_2D, id_);

	int mode = 0;

	switch (surface.GetImageFormat()) {
		case ImageFormat::RGBA32:
		case ImageFormat::RGBA8888: {
			mode = GL_RGBA;
			break;
		}
		case ImageFormat::RGB888: {
			mode = GL_RGB;
			break;
		}
		default: break;
	}

	// Linear resampling when scaling up and down.
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	const std::vector<std::uint8_t>& data{ surface.GetData() };

	GLSLType type{ GetType<decltype(std::remove_reference_t<decltype(data)>())::value_type>() };

	glTexImage2D(
		GL_TEXTURE_2D, 0, mode, size_.x, size_.y, 0, mode, static_cast<GLenum>(type), data.data()
	);

	glBindTexture(GL_TEXTURE_2D, 0);
}

TextureInstance::~TextureInstance() {
	glDeleteTextures(1, &id_);
}

} // namespace impl

Texture::Texture(const Surface& surface) {
	instance_ = std::shared_ptr<impl::TextureInstance>(new impl::TextureInstance(surface));
}

void Texture::Bind(std::uint32_t slot) const {
	std::int32_t max_texture_slots{ 0 };
	glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_texture_slots);
	PTGN_ASSERT(max_texture_slots != 0);
	PTGN_CHECK(
		slot < max_texture_slots, "Attempting to bind a slot outside of OpenGL texture slot minimum"
	);
	PTGN_CHECK(IsValid(), "Cannot bind texture which is destroyed or uninitialized");
	pglActiveTexture(GL_TEXTURE0 + slot);
	glBindTexture(GL_TEXTURE_2D, instance_->id_);
}

void Texture::Unbind() const {
	glBindTexture(GL_TEXTURE_2D, 0);
}

V2_int Texture::GetSize() const {
	PTGN_CHECK(IsValid(), "Cannot get size of texture which is destroyed or uninitialized");
	return instance_->size_;
}

} // namespace ptgn