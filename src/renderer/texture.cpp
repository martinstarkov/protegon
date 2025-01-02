#include "renderer/texture.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <type_traits>
#include <vector>

#include "core/game.h"
#include "core/window.h"
#include "event/event_handler.h"
#include "event/events.h"
#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "math/vector4.h"
#include "renderer/batch.h"
#include "renderer/color.h"
#include "renderer/gl_helper.h"
#include "renderer/gl_loader.h"
#include "renderer/gl_renderer.h"
#include "renderer/layer_info.h"
#include "renderer/renderer.h"
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

TextureInstance::TextureInstance(
	const void* pixel_data, const V2_int& size, TextureFormat format, TextureWrapping wrapping_x,
	TextureWrapping wrapping_y, TextureScaling minifying, TextureScaling magnifying, bool mipmaps,
	bool resize_with_window
) {
	GLCall(gl::glGenTextures(1, &id_));
	PTGN_ASSERT(id_ != 0, "Failed to generate texture using OpenGL context");

	PUSHSTATE();

	Bind();

	CreateTexture(pixel_data, size, format, wrapping_x, wrapping_y, minifying, magnifying, mipmaps);

	if (resize_with_window) {
		PTGN_ASSERT(
			pixel_data == nullptr,
			"Texture which resizes to window must be initialized with empty pixel data"
		);
		game.event.window.Subscribe(
			WindowEvent::Resized, this, std::function([this](const WindowResizedEvent&) {
				PUSHSTATE();

				Bind();

				CreateTexture(
					nullptr, game.window.GetSize(), format_, wrapping_x_, wrapping_y_, minifying_,
					magnifying_, mipmaps_
				);

				POPSTATE();
			})
		);
	}

	POPSTATE();
}

TextureInstance::~TextureInstance() {
	GLCall(gl::glDeleteTextures(1, &id_));

	game.event.window.Unsubscribe(this);
}

void TextureInstance::CreateTexture(
	const void* pixel_data, const V2_int& size, TextureFormat format, TextureWrapping wrapping_x,
	TextureWrapping wrapping_y, TextureScaling minifying, TextureScaling magnifying, bool mipmaps
) {
	SetData(pixel_data, size, format, 0);

	SetWrappingX(wrapping_x);
	SetWrappingY(wrapping_y);

	SetScalingMinifying(minifying);
	SetScalingMagnifying(magnifying);

	if (mipmaps) {
		GenerateMipmaps();
	} else {
		mipmaps_ = false;
	}
}

void TextureInstance::SetWrappingX(TextureWrapping x) {
	PTGN_ASSERT(IsBound(), "Cannot set wrapping of texture unless it is first bound");
	GLCall(gl::glTexParameteri(
		GL_TEXTURE_2D, static_cast<gl::GLenum>(impl::TextureParameter::WrapS), static_cast<int>(x)
	));
	wrapping_x_ = x;
}

void TextureInstance::SetWrappingY(TextureWrapping y) {
	PTGN_ASSERT(IsBound(), "Cannot set wrapping of texture unless it is first bound");
	GLCall(gl::glTexParameteri(
		GL_TEXTURE_2D, static_cast<gl::GLenum>(impl::TextureParameter::WrapT), static_cast<int>(y)
	));
	wrapping_y_ = y;
}

void TextureInstance::SetWrappingZ(TextureWrapping z) {
	PTGN_ASSERT(IsBound(), "Cannot set wrapping of texture unless it is first bound");
	GLCall(gl::glTexParameteri(
		GL_TEXTURE_2D, static_cast<gl::GLenum>(impl::TextureParameter::WrapR), static_cast<int>(z)
	));
	wrapping_z_ = z;
}

void TextureInstance::SetScalingMagnifying(TextureScaling magnifying) {
	PTGN_ASSERT(IsBound(), "Cannot set magnifying scaling of texture unless it is first bound");
	GLCall(gl::glTexParameteri(
		GL_TEXTURE_2D, static_cast<gl::GLenum>(impl::TextureParameter::MagScaling),
		static_cast<int>(magnifying)
	));
	magnifying_ = magnifying;
}

void TextureInstance::SetScalingMinifying(TextureScaling minifying) {
	PTGN_ASSERT(IsBound(), "Cannot set minifying scaling of texture unless it is first bound");
	GLCall(gl::glTexParameteri(
		GL_TEXTURE_2D, static_cast<gl::GLenum>(impl::TextureParameter::MinScaling),
		static_cast<int>(minifying)
	));
	minifying_ = minifying;
}

void TextureInstance::GenerateMipmaps() {
	PTGN_ASSERT(
		ValidMinifyingForMipmaps(minifying_),
		"Set texture minifying scaling to mipmap type before generating mipmaps"
	);
	PTGN_ASSERT(IsBound(), "Cannot generate mipmaps for texture unless it is first bound");
	GLCall(gl::GenerateMipmap(GL_TEXTURE_2D));
	mipmaps_ = true;
}

void TextureInstance::Bind() const {
	GLCall(gl::glBindTexture(GL_TEXTURE_2D, id_));
}

bool TextureInstance::IsBound() const {
	return Texture::GetBoundId() == static_cast<std::int32_t>(id_);
}

bool TextureInstance::ValidMinifyingForMipmaps(TextureScaling minifying) {
	return minifying == TextureScaling::LinearMipmapLinear ||
		   minifying == TextureScaling::LinearMipmapNearest ||
		   minifying == TextureScaling::NearestMipmapLinear ||
		   minifying == TextureScaling::NearestMipmapNearest;
}

void TextureInstance::SetData(
	const void* pixel_data, const V2_int& size, TextureFormat format, int mipmap_level
) {
	PTGN_ASSERT(
		format != TextureFormat::Unknown, "Cannot set data for texture with unknown texture format"
	);

	PTGN_ASSERT(IsBound(), "Cannot set data for texture unless it is first bound");

	size_	= size;
	format_ = format;

	auto formats{ impl::GetGLFormats(format) };

	GLCall(gl::glTexImage2D(
		GL_TEXTURE_2D, mipmap_level, static_cast<gl::GLint>(formats.internal_), size_.x, size_.y, 0,
		formats.format_, static_cast<gl::GLenum>(impl::GLType::UnsignedByte), pixel_data
	));
}

GLFormats GetGLFormats(TextureFormat format) {
	// Possible internal format options:
	// GL_R#size, GL_RG#size, GL_RGB#size, GL_RGBA#size
	switch (format) {
		case TextureFormat::RGBA8888: {
			return { InternalGLFormat::RGBA8, GL_RGBA, 4 };
		}
		case TextureFormat::RGB888: {
			return { InternalGLFormat::RGB8, GL_RGB, 3 };
		}
#ifdef __EMSCRIPTEN__
		case TextureFormat::BGRA8888:
		case TextureFormat::BGR888:	  {
			PTGN_ERROR("OpenGL ES3.0 does not support BGR(A) texture formats in glTexImage2D");
		}
#else
		case TextureFormat::BGRA8888: {
			return { InternalGLFormat::RGBA8, GL_BGRA, 4 };
		}
		case TextureFormat::BGR888: {
			return { InternalGLFormat::RGB8, GL_BGR, 3 };
		}
#endif
		default: break;
	}
	PTGN_ERROR("Could not determine OpenGL formats for given TextureFormat");
}

} // namespace impl

Texture::Texture(
	const void* pixel_data, const V2_int& size, TextureFormat format, TextureWrapping wrapping_x,
	TextureWrapping wrapping_y, TextureScaling minifying, TextureScaling magnifying, bool mipmaps,
	bool resize_with_window
) {
	Create(
		pixel_data, size, format, wrapping_x, wrapping_y, minifying, magnifying, mipmaps,
		resize_with_window
	);
}

Texture::Texture(const path& image_path) :
	Texture{ std::invoke([&]() {
		PTGN_ASSERT(
			FileExists(image_path),
			"Cannot create texture from file path which does not exist: ", image_path.string()
		);
		Surface s{ image_path };
		PTGN_ASSERT(
			s.GetFormat() != TextureFormat::Unknown,
			"Cannot create texture with unknown texture format"
		);
		return s;
	}) } {}

Texture::Texture(const Surface& surface) : Texture{ surface.GetData(), surface.GetSize() } {}

Texture::Texture(bool resize_with_window) :
	Texture{ nullptr,
			 game.window.GetSize(),
			 default_format,
			 default_wrapping,
			 default_wrapping,
			 default_minifying_scaling,
			 default_magnifying_scaling,
			 false,
			 resize_with_window } {}

Texture::Texture(const V2_float& size) :
	Texture{ nullptr,
			 size,
			 default_format,
			 default_wrapping,
			 default_wrapping,
			 default_minifying_scaling,
			 default_magnifying_scaling,
			 false,
			 false } {}

Color Texture::GetPixel(const V2_int& coordinate) const {
	PTGN_ASSERT(IsValid(), "Cannot retrieve pixel of invalid texture");
	V2_int size{ GetSize() };
	PTGN_ASSERT(
		coordinate.x >= 0 && coordinate.x < size.x,
		"Cannot get pixel out of range of frame buffer texture"
	);
	PTGN_ASSERT(
		coordinate.y >= 0 && coordinate.y < size.y,
		"Cannot get pixel out of range of frame buffer texture"
	);
	auto formats{ impl::GetGLFormats(GetFormat()) };
	PTGN_ASSERT(
		formats.components_ >= 3,
		"Cannot retrieve pixel data of texture with less than 3 RGB components"
	);
	std::vector<std::uint8_t> v(formats.components_ * 1 * 1);
	int y{ size.y - 1 - coordinate.y };
	PTGN_ASSERT(y >= 0);
	GLCall(gl::glReadPixels(
		coordinate.x, y, 1, 1, formats.format_, static_cast<gl::GLenum>(impl::GLType::UnsignedByte),
		(void*)v.data()
	));
	return Color{ v[0], v[1], v[2],
				  formats.components_ == 4 ? v[3] : static_cast<std::uint8_t>(255) };
}

void Texture::ForEachPixel(const std::function<void(V2_int, Color)>& func) const {
	PTGN_ASSERT(IsValid(), "Cannot retrieve pixels of invalid texture");
	V2_int size{ GetSize() };
	auto formats{ impl::GetGLFormats(GetFormat()) };

	PTGN_ASSERT(
		formats.components_ >= 3,
		"Cannot retrieve pixel data of texture with less than 3 RGB components"
	);

	std::vector<std::uint8_t> v(formats.components_ * size.x * size.y);
	GLCall(gl::glReadPixels(
		0, 0, size.x, size.y, formats.format_, static_cast<gl::GLenum>(impl::GLType::UnsignedByte),
		(void*)v.data()
	));
	for (int j{ 0 }; j < size.y; j++) {
		// Ensure left-to-right and top-to-bottom iteration.
		int row{ (size.y - 1 - j) * size.x * formats.components_ };
		for (int i{ 0 }; i < size.x; i++) {
			int idx{ row + i * formats.components_ };
			PTGN_ASSERT(static_cast<std::size_t>(idx) < v.size());
			Color color{ v[idx], v[idx + 1], v[idx + 2],
						 formats.components_ == 4 ? v[idx + 3] : static_cast<std::uint8_t>(255) };
			std::invoke(func, V2_int{ i, j }, color);
		}
	}
}

Texture::Texture(const std::vector<Color>& pixels, const V2_int& size) :
	Texture{ std::invoke([&]() {
				 PTGN_ASSERT(
					 pixels.size() == size.x * size.y,
					 "Provided pixel array must match texture size"
				 );
				 return (void*)pixels.data();
			 }),
			 size } {}

void Texture::Draw(
	const Rect& destination, const TextureInfo& texture_info
) const {
	Draw(destination, texture_info, {});
}

void Texture::Draw(
	const Rect& destination, const TextureInfo& texture_info, const LayerInfo& layer_info
) const {
	PTGN_ASSERT(IsValid(), "Cannot draw uninitialized or destroyed texture");

	Rect dest{ destination };

	if (dest.IsZero()) {
		dest = Rect::Fullscreen();
	} else if (dest.size.IsZero()) {
		dest.size = GetSize();
	}

	game.renderer.data_.AddTexture(
		dest.GetVertices(texture_info.rotation_center), *this,
		impl::RenderData::GetTextureCoordinates(
			texture_info.source_position, texture_info.source_size, GetSize(), texture_info.flip
		),
		texture_info.tint.Normalized(), layer_info.z_index, layer_info.render_layer
	);
}

void Texture::Bind() const {
	PTGN_ASSERT(IsValid(), "Cannot bind invalid or uninitialized texture");
	Get().Bind();
#ifdef PTGN_DEBUG
	++game.stats.texture_binds;
#endif
}

void Texture::Unbind(std::uint32_t slot) {
	SetActiveSlot(slot);
	GLCall(gl::glBindTexture(GL_TEXTURE_2D, 0));
}

void Texture::Bind(std::uint32_t slot) const {
	SetActiveSlot(slot);
	Bind();
}

void Texture::SetActiveSlot(std::uint32_t slot) {
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

bool Texture::IsBound() const {
	return IsValid() && Get().IsBound();
}

void Texture::SetSubData(
	const void* pixel_data, TextureFormat format, const V2_int& offset, const V2_int& size,
	int mipmap_level
) {
	PTGN_ASSERT(IsValid(), "Cannot set sub data of invalid or uninitialized texture");
	PTGN_ASSERT(pixel_data != nullptr);

	PUSHSTATE();

	auto formats{ impl::GetGLFormats(format) };

	auto& i{ Get() };
	i.Bind();

	GLCall(gl::glTexSubImage2D(
		GL_TEXTURE_2D, mipmap_level, offset.x, offset.y, size.x, size.y, formats.format_,
		static_cast<gl::GLenum>(impl::GLType::UnsignedByte), pixel_data
	));

	POPSTATE();
}

std::int32_t Texture::GetActiveSlot() {
	std::int32_t id{ -1 };
	GLCall(gl::glGetIntegerv(GL_ACTIVE_TEXTURE, &id));
	PTGN_ASSERT(id >= 0);
	return id;
}

void Texture::SetSubData(
	const std::vector<Color>& pixels, const V2_int& offset, const V2_int& size, int mipmap_level
) {
	PTGN_ASSERT(IsValid(), "Cannot set sub data of invalid or uninitialized texture");
	PTGN_ASSERT(
		pixels.size() == GetSize().x * GetSize().y, "Provided pixel array must match texture size"
	);

	SetSubData((void*)pixels.data(), TextureFormat::RGBA8888, offset, size, mipmap_level);
}

V2_int Texture::GetSize() const {
	PTGN_ASSERT(IsValid(), "Cannot size format of invalid or uninitialized texture");
	return Get().size_;
}

TextureFormat Texture::GetFormat() const {
	PTGN_ASSERT(IsValid(), "Cannot get format of invalid or uninitialized texture");
	return Get().format_;
}

void Texture::SetWrappingX(TextureWrapping x) {
	PTGN_ASSERT(IsValid(), "Cannot set wrapping of invalid or uninitialized texture");

	PUSHSTATE();

	auto& i{ Get() };
	i.Bind();
	i.SetWrappingX(x);

	POPSTATE();
}

void Texture::SetWrappingY(TextureWrapping y) {
	PTGN_ASSERT(IsValid(), "Cannot set wrapping of invalid or uninitialized texture");

	PUSHSTATE();

	auto& i{ Get() };
	i.Bind();
	i.SetWrappingY(y);

	POPSTATE();
}

void Texture::SetWrappingZ(TextureWrapping z) {
	PTGN_ASSERT(IsValid(), "Cannot set wrapping of invalid or uninitialized texture");

	PUSHSTATE();

	auto& i{ Get() };
	i.Bind();
	i.SetWrappingZ(z);

	POPSTATE();
}

void Texture::SetScalingMagnifying(TextureScaling magnifying) {
	PTGN_ASSERT(IsValid(), "Cannot set scaling of invalid or uninitialized texture");

	PUSHSTATE();

	auto& i{ Get() };
	i.Bind();
	i.SetScalingMagnifying(magnifying);

	POPSTATE();
}

void Texture::SetScalingMinifying(TextureScaling minifying) {
	PTGN_ASSERT(IsValid(), "Cannot set scaling of invalid or uninitialized texture");

	PUSHSTATE();

	auto& i{ Get() };
	i.Bind();
	i.SetScalingMinifying(minifying);

	POPSTATE();
}

void Texture::SetClampBorderColor(const Color& color) const {
	PTGN_ASSERT(IsValid(), "Cannot set clamp border color of invalid or uninitialized texture");

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

void Texture::GenerateMipmaps() {
	PTGN_ASSERT(IsValid(), "Cannot generate mipmaps for invalid or uninitialized texture");

	PUSHSTATE();

	auto& i{ Get() };
	i.Bind();
	i.GenerateMipmaps();

	POPSTATE();
}

} // namespace ptgn
