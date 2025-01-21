#include "renderer/texture.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

#include "core/game.h"
#include "core/window.h"
#include "event/event_handler.h"
#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "math/vector4.h"
#include "renderer/batch.h"
#include "renderer/color.h"
#include "renderer/flip.h"
#include "renderer/gl_helper.h"
#include "renderer/gl_loader.h"
#include "renderer/gl_renderer.h"
#include "renderer/renderer.h"
#include "renderer/shader.h"
#include "renderer/surface.h"
#include "scene/camera.h"
#include "utility/debug.h"
#include "utility/file.h"
#include "utility/handle.h"
#include "utility/log.h"
#include "utility/utility.h"

namespace ptgn {

std::array<V2_float, 4> TextureInfo::GetTextureCoordinates(
	const V2_float& texture_size, bool offset_texels
) const {
	PTGN_ASSERT(texture_size.x > 0.0f, "Texture must have width > 0");
	PTGN_ASSERT(texture_size.y > 0.0f, "Texture must have height > 0");

	PTGN_ASSERT(
		source_position.x < texture_size.x, "Source position X must be within texture width"
	);
	PTGN_ASSERT(
		source_position.y < texture_size.y, "Source position Y must be within texture height"
	);

	auto size{ source_size };

	if (size.IsZero()) {
		size = texture_size - source_position;
	}

	// Convert to 0 -> 1 range.
	V2_float src_pos{ source_position / texture_size };
	V2_float src_size{ size / texture_size };

	if (src_size.x > 1.0f || src_size.y > 1.0f) {
		PTGN_WARN("Drawing source size from outside of texture size");
	}

	V2_float half_pixel{ (offset_texels ? 0.5f : 0.0f) / texture_size };

	std::array<V2_float, 4> texture_coordinates{
		src_pos + half_pixel,
		V2_float{ src_pos.x + src_size.x - half_pixel.x, src_pos.y + half_pixel.y },
		src_pos + src_size - half_pixel,
		V2_float{ src_pos.x + half_pixel.x, src_pos.y + src_size.y - half_pixel.y },
	};

	return texture_coordinates;
}

void TextureInfo::FlipTextureCoordinates(std::array<V2_float, 4>& texture_coords, Flip flip) {
	const auto flip_x = [&]() {
		std::swap(texture_coords[0].x, texture_coords[1].x);
		std::swap(texture_coords[2].x, texture_coords[3].x);
	};
	const auto flip_y = [&]() {
		std::swap(texture_coords[0].y, texture_coords[3].y);
		std::swap(texture_coords[1].y, texture_coords[2].y);
	};
	switch (flip) {
		case Flip::None:	   break;
		case Flip::Horizontal: flip_x(); break;
		case Flip::Vertical:   flip_y(); break;
		case Flip::Both:
			flip_x();
			flip_y();
			break;
		default: PTGN_ERROR("Unrecognized flip state");
	}
}

namespace impl {

TextureInstance::TextureInstance(
	const void* pixel_data, const V2_int& size, TextureFormat format, TextureWrapping wrapping_x,
	TextureWrapping wrapping_y, TextureScaling minifying, TextureScaling magnifying, bool mipmaps,
	bool resize_with_window
) {
	GLCall(gl::glGenTextures(1, &id_));
	PTGN_ASSERT(id_ != 0, "Failed to generate texture using OpenGL context");
#ifdef GL_ANNOUNCE_TEXTURE_CALLS
	PTGN_LOG("GL: Generated texture with id ", id_);
#endif

	std::uint32_t restore_id{ Texture::GetBoundId() };

	Bind();

	CreateTexture(pixel_data, size, format, wrapping_x, wrapping_y, minifying, magnifying, mipmaps);

	if (resize_with_window) {
		PTGN_ASSERT(
			pixel_data == nullptr,
			"Texture which resizes to window must be initialized with empty pixel data"
		);
		game.event.window.Subscribe(
			WindowEvent::Resized, this, std::function([this](const WindowResizedEvent&) {
				std::uint32_t restore_id{ Texture::GetBoundId() };

				Bind();

				CreateTexture(
					nullptr, game.window.GetSize(), format_, wrapping_x_, wrapping_y_, minifying_,
					magnifying_, mipmaps_
				);

				Texture::BindId(restore_id);
			})
		);
	}

	Texture::BindId(restore_id);
}

TextureInstance::~TextureInstance() {
	GLCall(gl::glDeleteTextures(1, &id_));
#ifdef GL_ANNOUNCE_TEXTURE_CALLS
	PTGN_LOG("GL: Deleted texture with id ", id_);
#endif

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
	Texture::BindId(id_);
}

bool TextureInstance::IsBound() const {
	return Texture::GetBoundId() == id_;
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
		case TextureFormat::Unknown: [[fallthrough]];
#endif
		default: PTGN_ERROR("Could not determine OpenGL formats for given TextureFormat");
	}
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

Texture::Texture(const std::vector<Color>& pixels, const V2_int& size) :
	Texture{ std::invoke([&]() {
				 PTGN_ASSERT(
					 pixels.size() == static_cast<std::size_t>(size.x * size.y),
					 "Provided pixel array must match texture size"
				 );
				 return static_cast<const void*>(pixels.data());
			 }),
			 size } {}

Texture::Texture(const Surface& surface) : Texture{ surface.GetData(), surface.GetSize() } {}

Texture::Texture([[maybe_unused]] const WindowTexture& window_texture) :
	Texture{ nullptr,
			 game.window.GetSize(),
			 default_format,
			 default_wrapping,
			 default_wrapping,
			 default_minifying_scaling,
			 default_magnifying_scaling,
			 false,
			 true } {}

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

void Texture::Draw(Rect destination, const TextureInfo& texture_info, std::int32_t render_layer)
	const {
	PTGN_ASSERT(IsValid(), "Cannot draw uninitialized or destroyed texture");

	if (destination.IsZero()) {
		destination = Rect::Fullscreen();
	} else if (destination.size.IsZero()) {
		destination.size = GetSize();
	}

	auto vertices{ destination.GetVertices(texture_info.rotation_center) };

	auto tex_coords{ texture_info.GetTextureCoordinates(GetSize()) };

	TextureInfo::FlipTextureCoordinates(tex_coords, texture_info.flip);

	game.renderer.GetRenderData().AddPrimitiveQuad(
		vertices, render_layer, texture_info.tint.Normalized(), tex_coords, *this
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
	BindId(0);
}

void Texture::BindId(std::uint32_t id) {
#ifdef GL_ANNOUNCE_TEXTURE_CALLS
	PTGN_LOG("GL: Bound texture with id ", id);
#endif
	GLCall(gl::glBindTexture(GL_TEXTURE_2D, id));
}

void Texture::Bind(std::uint32_t slot) const {
	SetActiveSlot(slot);
	Bind();
}

void Texture::SetActiveSlot(std::uint32_t slot) {
	PTGN_ASSERT(
		slot < GLRenderer::GetMaxTextureSlots(),
		"Attempting to bind a slot outside of OpenGL texture slot maximum"
	);
#ifdef GL_ANNOUNCE_TEXTURE_CALLS
	PTGN_LOG("GL: Set active texture slot to ", slot);
#endif
	GLCall(gl::ActiveTexture(GL_TEXTURE0 + slot));
}

std::uint32_t Texture::GetBoundId() {
	std::int32_t id{ -1 };
	GLCall(gl::glGetIntegerv(static_cast<gl::GLenum>(impl::GLBinding::Texture2D), &id));
	PTGN_ASSERT(id >= 0, "Failed to retrieve bound texture id");
	return static_cast<std::uint32_t>(id);
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

	std::uint32_t restore_id{ Texture::GetBoundId() };

	auto formats{ impl::GetGLFormats(format) };

	auto& i{ Get() };
	i.Bind();

	GLCall(gl::glTexSubImage2D(
		GL_TEXTURE_2D, mipmap_level, offset.x, offset.y, size.x, size.y, formats.format_,
		static_cast<gl::GLenum>(impl::GLType::UnsignedByte), pixel_data
	));

	Texture::BindId(restore_id);
}

std::uint32_t Texture::GetActiveSlot() {
	std::int32_t id{ -1 };
	GLCall(gl::glGetIntegerv(GL_ACTIVE_TEXTURE, &id));
	PTGN_ASSERT(id >= 0, "Failed to retrieve the currently active texture slot");
	return static_cast<std::uint32_t>(id);
}

void Texture::SetSubData(
	const std::vector<Color>& pixels, const V2_int& offset, const V2_int& size, int mipmap_level
) {
	PTGN_ASSERT(IsValid(), "Cannot set sub data of invalid or uninitialized texture");
	PTGN_ASSERT(
		pixels.size() == static_cast<std::size_t>(GetSize().x * GetSize().y),
		"Provided pixel array must match texture size"
	);

	SetSubData(
		static_cast<const void*>(pixels.data()), TextureFormat::RGBA8888, offset, size, mipmap_level
	);
}

V2_int Texture::GetSize() const {
	PTGN_ASSERT(IsValid(), "Cannot size format of invalid or uninitialized texture");
	return Get().size_;
}

TextureFormat Texture::GetFormat() const {
	PTGN_ASSERT(IsValid(), "Cannot get format of invalid or uninitialized texture");
	return Get().format_;
}

void Texture::Draw(Rect destination, TextureInfo texture_info, Shader shader, const Camera& camera)
	const {
	PTGN_ASSERT(IsValid(), "Cannot draw uninitialized or destroyed texture");

	if (*this == game.renderer.screen_target_.GetTexture()) {
		FrameBuffer::Unbind();
	} else {
		game.renderer.Flush();
	}

	if (shader == Shader{}) {
		shader = game.shader.Get(ScreenShader::Default);
	}

	if (destination.IsZero()) {
		destination = Rect::Fullscreen();
	} else if (destination.size.IsZero()) {
		destination.size = GetSize();
	}

	// Shaders coordinates are in bottom right instead of top left.
	if (texture_info.flip == Flip::Vertical) {
		texture_info.flip = Flip::None;
	} else if (texture_info.flip == Flip::Both) {
		texture_info.flip = Flip::Horizontal;
	} else if (texture_info.flip == Flip::None) {
		texture_info.flip = Flip::Vertical;
	}

	auto positions{ destination.GetVertices(texture_info.rotation_center) };

	auto tex_coords{ texture_info.GetTextureCoordinates(GetSize()) };

	TextureInfo::FlipTextureCoordinates(tex_coords, texture_info.flip);

	constexpr std::size_t index_count{ 6 };

	std::array<QuadVertex, 4> vertices{};

	V4_float color{ texture_info.tint.Normalized() };

	for (std::size_t i{ 0 }; i < vertices.size(); i++) {
		vertices[i].position  = { positions[i].x, positions[i].y, 0.0f };
		vertices[i].color	  = { color.x, color.y, color.z, color.w };
		vertices[i].tex_coord = { tex_coords[i].x, tex_coords[i].y };
		vertices[i].tex_index = { 0.0f };
	}

	shader.Bind();
	shader.SetUniform("u_ViewProjection", camera);
	shader.SetUniform("u_Resolution", V2_float{ game.window.GetSize() });
	shader.SetUniform("u_Texture", 0);

	auto vao{ game.renderer.GetVertexArray<impl::BatchType::Quad>() };
	vao.Bind();

	Bind(0);

	vao.GetVertexBuffer().SetSubData(
		vertices.data(), static_cast<std::uint32_t>(Sizeof(vertices)), false
	);
	vao.Draw(index_count, false);
}

void Texture::SetWrappingX(TextureWrapping x) {
	PTGN_ASSERT(IsValid(), "Cannot set wrapping of invalid or uninitialized texture");

	std::uint32_t restore_id{ Texture::GetBoundId() };

	auto& i{ Get() };
	i.Bind();
	i.SetWrappingX(x);

	Texture::BindId(restore_id);
}

void Texture::SetWrappingY(TextureWrapping y) {
	PTGN_ASSERT(IsValid(), "Cannot set wrapping of invalid or uninitialized texture");

	std::uint32_t restore_id{ Texture::GetBoundId() };

	auto& i{ Get() };
	i.Bind();
	i.SetWrappingY(y);

	Texture::BindId(restore_id);
}

void Texture::SetWrappingZ(TextureWrapping z) {
	PTGN_ASSERT(IsValid(), "Cannot set wrapping of invalid or uninitialized texture");

	std::uint32_t restore_id{ Texture::GetBoundId() };

	auto& i{ Get() };
	i.Bind();
	i.SetWrappingZ(z);

	Texture::BindId(restore_id);
}

void Texture::SetScalingMagnifying(TextureScaling magnifying) {
	PTGN_ASSERT(IsValid(), "Cannot set scaling of invalid or uninitialized texture");

	std::uint32_t restore_id{ Texture::GetBoundId() };

	auto& i{ Get() };
	i.Bind();
	i.SetScalingMagnifying(magnifying);

	Texture::BindId(restore_id);
}

void Texture::SetScalingMinifying(TextureScaling minifying) {
	PTGN_ASSERT(IsValid(), "Cannot set scaling of invalid or uninitialized texture");

	std::uint32_t restore_id{ Texture::GetBoundId() };

	auto& i{ Get() };
	i.Bind();
	i.SetScalingMinifying(minifying);

	Texture::BindId(restore_id);
}

void Texture::SetClampBorderColor(const Color& color) const {
	PTGN_ASSERT(IsValid(), "Cannot set clamp border color of invalid or uninitialized texture");

	std::uint32_t restore_id{ Texture::GetBoundId() };

	Bind();

	V4_float c{ color.Normalized() };
	std::array<float, 4> border_color{ c.x, c.y, c.z, c.w };

	GLCall(gl::glTexParameterfv(
		GL_TEXTURE_2D, static_cast<gl::GLenum>(impl::TextureParameter::BorderColor),
		border_color.data()
	));

	Texture::BindId(restore_id);
}

void Texture::GenerateMipmaps() {
	PTGN_ASSERT(IsValid(), "Cannot generate mipmaps for invalid or uninitialized texture");

	std::uint32_t restore_id{ Texture::GetBoundId() };

	auto& i{ Get() };
	i.Bind();
	i.GenerateMipmaps();

	Texture::BindId(restore_id);
}

} // namespace ptgn
