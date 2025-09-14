#include "renderer/texture.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "SDL_error.h"
#include "SDL_image.h"
#include "SDL_pixels.h"
#include "SDL_surface.h"
#include "common/assert.h"
#include "components/generic.h"
#include "core/entity.h"
#include "core/game.h"
#include "debug/config.h"
#include "debug/debug_system.h"
#include "debug/log.h"
#include "debug/stats.h"
#include "math/vector2.h"
#include "math/vector4.h"
#include "renderer/api/color.h"
#include "renderer/api/flip.h"
#include "renderer/buffers/frame_buffer.h"
#include "renderer/gl/gl_helper.h"
#include "renderer/gl/gl_loader.h"
#include "renderer/gl/gl_renderer.h"
#include "renderer/gl/gl_types.h"
#include "resources/resource_manager.h"
#include "utility/file.h"

namespace ptgn {

Color GetPixel(const path& texture_filepath, const V2_int& coordinate) {
	impl::Surface s{ texture_filepath };
	return s.GetPixel(coordinate);
}

V2_int ForEachPixel(
	const path& texture_filepath, const std::function<void(const V2_int&, const Color&)>& function
) {
	impl::Surface s{ texture_filepath };
	s.ForEachPixel(function);
	return s.size;
}

namespace impl {

TextureFormat GetFormatFromOpenGL(InternalGLFormat opengl_internal_format) {
	switch (opengl_internal_format) {
		case InternalGLFormat::RGBA8:	 return TextureFormat::RGBA8888;
		case InternalGLFormat::RGB8:	 return TextureFormat::RGB888;
		case InternalGLFormat::R8:		 return TextureFormat::A8;
		case InternalGLFormat::HDR_RGBA: return TextureFormat::HDR_RGBA;
		case InternalGLFormat::HDR_RGB:	 return TextureFormat::HDR_RGB;
		default:						 PTGN_ERROR("Unsupported or unrecognized OpenGL format");
	}
}

TextureFormat GetFormatFromSDL(std::uint32_t sdl_format) {
	switch (sdl_format) {
		case SDL_PIXELFORMAT_RGBA8888: return TextureFormat::RGBA8888;
		case SDL_PIXELFORMAT_ABGR8888: return TextureFormat::ABGR8888;
		case SDL_PIXELFORMAT_RGB24:	   [[fallthrough]];
		case SDL_PIXELFORMAT_RGB888:   return TextureFormat::RGB888;
		case SDL_PIXELFORMAT_ARGB8888: return TextureFormat::ARGB8888;
		case SDL_PIXELFORMAT_BGRA8888: return TextureFormat::BGRA8888;
		case SDL_PIXELFORMAT_BGR24:	   [[fallthrough]];
		case SDL_PIXELFORMAT_BGR888:   return TextureFormat::BGR888;
		case SDL_PIXELFORMAT_INDEX8:   return TextureFormat::A8;
		case SDL_PIXELFORMAT_UNKNOWN:  return TextureFormat::Unknown;
		/*
		case SDL_PIXELFORMAT_INDEX1LSB: [[fallthrough]];
		case SDL_PIXELFORMAT_INDEX1MSB: [[fallthrough]];
#ifndef __EMSCRIPTEN__
		case SDL_PIXELFORMAT_INDEX2LSB: [[fallthrough]];
		case SDL_PIXELFORMAT_INDEX2MSB: [[fallthrough]];
#endif
		case SDL_PIXELFORMAT_INDEX4LSB:	  [[fallthrough]];
		case SDL_PIXELFORMAT_INDEX4MSB:	  [[fallthrough]];
		case SDL_PIXELFORMAT_RGB332:	  [[fallthrough]];
		case SDL_PIXELFORMAT_RGB444:	  [[fallthrough]];
		case SDL_PIXELFORMAT_BGR444:	  [[fallthrough]];
		case SDL_PIXELFORMAT_RGB555:	  [[fallthrough]];
		case SDL_PIXELFORMAT_BGR555:	  [[fallthrough]];
		case SDL_PIXELFORMAT_ARGB4444:	  [[fallthrough]];
		case SDL_PIXELFORMAT_RGBA4444:	  [[fallthrough]];
		case SDL_PIXELFORMAT_ABGR4444:	  [[fallthrough]];
		case SDL_PIXELFORMAT_BGRA4444:	  [[fallthrough]];
		case SDL_PIXELFORMAT_ARGB1555:	  [[fallthrough]];
		case SDL_PIXELFORMAT_RGBA5551:	  [[fallthrough]];
		case SDL_PIXELFORMAT_ABGR1555:	  [[fallthrough]];
		case SDL_PIXELFORMAT_BGRA5551:	  [[fallthrough]];
		case SDL_PIXELFORMAT_RGB565:	  [[fallthrough]];
		case SDL_PIXELFORMAT_BGR565:	  [[fallthrough]];
		case SDL_PIXELFORMAT_RGBX8888:	  [[fallthrough]];
		case SDL_PIXELFORMAT_BGRX8888:	  [[fallthrough]];
		case SDL_PIXELFORMAT_ARGB2101010: [[fallthrough]];
		*/
		default:					   PTGN_ERROR("Unsupported or unrecognized SDL format");
	}
}

GLFormats GetGLFormats(TextureFormat format) {
	// Possible internal format options:
	// GL_R#size, GL_RG#size, GL_RGB#size, GL_RGBA#size
	switch (format) {
		case TextureFormat::ABGR8888: [[fallthrough]];
		case TextureFormat::RGBA8888: {
			return { InternalGLFormat::RGBA8, InputGLFormat::RGBA, 4 };
		}
		case TextureFormat::HDR_RGBA: {
			return { InternalGLFormat::HDR_RGBA, InputGLFormat::RGBA, 4 };
		}
		case TextureFormat::HDR_RGB: {
			return { InternalGLFormat::HDR_RGB, InputGLFormat::RGB, 3 };
		}
		case TextureFormat::RGB888: {
			return { InternalGLFormat::RGB8, InputGLFormat::RGB, 3 };
		}
#ifdef __EMSCRIPTEN__
		case TextureFormat::BGRA8888:
		case TextureFormat::BGR888:	  {
			PTGN_ERROR("OpenGL ES3.0 does not support BGR(A) texture formats in glTexImage2D");
		}
#else
		case TextureFormat::ARGB8888: [[fallthrough]];
		case TextureFormat::BGRA8888: {
			return { InternalGLFormat::RGBA8, InputGLFormat::BGRA, 4 };
		}
		case TextureFormat::BGR888: {
			return { InternalGLFormat::RGB8, InputGLFormat::BGR, 3 };
		}
		case TextureFormat::A8: {
			return { InternalGLFormat::R8, InputGLFormat::SingleChannel, 1 };
		}
		case TextureFormat::Depth24: {
			return { InternalGLFormat::DEPTH24, InputGLFormat::Depth, 0 };
		}
		case TextureFormat::Depth24_Stencil8: {
			return { InternalGLFormat::DEPTH24_STENCIL8, InputGLFormat::DepthStencil, 0 };
		}
#endif
		default:
			PTGN_ERROR("Could not determine compatible OpenGL formats for given TextureFormat");
	}
}

std::array<V2_float, 4> GetTextureCoordinates(
	const V2_float& source_position, const V2_float& source_size, const V2_float& texture_size,
	bool offset_texels
) {
	PTGN_ASSERT(texture_size.x > 0.0f, "Texture must have width > 0");
	PTGN_ASSERT(texture_size.y > 0.0f, "Texture must have height > 0");

	PTGN_ASSERT(
		source_position.x < texture_size.x, "Source position X must be within texture width"
	);
	PTGN_ASSERT(
		source_position.y < texture_size.y, "Source position Y must be within texture height"
	);

	V2_float size{ source_size };

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

void FlipTextureCoordinates(std::array<V2_float, 4>& texture_coords, Flip flip) {
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

Texture::Texture(const Surface& surface) :
	Texture{ static_cast<const void*>(surface.data.data()), surface.size, surface.format } {}

Texture::Texture(
	const void* data, const V2_int& size, TextureFormat format, int mipmap_level,
	TextureWrapping wrapping_x, TextureWrapping wrapping_y, TextureScaling minifying,
	TextureScaling magnifying, bool mipmaps
) {
	GenerateTexture();
	TextureId restore_texture_id{ Texture::GetBoundId() };
	Bind();
	SetData(data, size, format, mipmap_level);
	SetParameterI(TextureParameter::WrapS, static_cast<int>(wrapping_x));
	SetParameterI(TextureParameter::WrapT, static_cast<int>(wrapping_y));
	SetParameterI(TextureParameter::MinifyingScaling, static_cast<int>(minifying));
	SetParameterI(TextureParameter::MagnifyingScaling, static_cast<int>(magnifying));
	if (mipmaps) {
		GenerateMipmaps();
	}
	Texture::BindId(restore_texture_id);
}

Texture::Texture(Texture&& other) noexcept :
	id_{ std::exchange(other.id_, 0) },
	size_{ std::exchange(other.size_, {}) },
	format_{ std::exchange(other.format_, { TextureFormat::Unknown }) } {}

Texture& Texture::operator=(Texture&& other) noexcept {
	if (this != &other) {
		DeleteTexture();
		id_		= std::exchange(other.id_, 0);
		size_	= std::exchange(other.size_, {});
		format_ = std::exchange(other.format_, { TextureFormat::Unknown });
	}
	return *this;
}

Texture::~Texture() {
	DeleteTexture();
}

void Texture::GenerateTexture() {
	GLCall(glGenTextures(1, &id_));
	PTGN_ASSERT(IsValid(), "Failed to generate texture using OpenGL context");
#ifdef GL_ANNOUNCE_TEXTURE_CALLS
	PTGN_LOG("GL: Generated texture with id ", id_);
#endif
}

void Texture::DeleteTexture() noexcept {
	if (!IsValid()) {
		return;
	}
	GLCall(glDeleteTextures(1, &id_));
#ifdef GL_ANNOUNCE_TEXTURE_CALLS
	PTGN_LOG("GL: Deleted texture with id ", id_);
#endif
	id_		= 0;
	size_	= {};
	format_ = TextureFormat::Unknown;
}

TextureFormat Texture::GetFormat() const {
	return format_;
}

V2_int Texture::GetSize() const {
	return size_;
}

void Texture::SetParameterI(TextureParameter parameter, std::int32_t value) const {
	PTGN_ASSERT(IsBound(), "Texture must be bound prior to setting its parameters");
	PTGN_ASSERT(value != -1, "Cannot set texture parameter value to -1");
	GLCall(glTexParameteri(
		static_cast<GLenum>(TextureTarget::Texture2D), static_cast<GLenum>(parameter), value
	));
}

std::int32_t Texture::GetParameterI(TextureParameter parameter) const {
	PTGN_ASSERT(IsBound(), "Texture must be bound prior to getting its parameters");
	std::int32_t value{ -1 };
	GLCall(glGetTexParameteriv(
		static_cast<GLenum>(TextureTarget::Texture2D), static_cast<GLenum>(parameter), &value
	));
	PTGN_ASSERT(value != -1, "Failed to retrieve texture parameter");
	return value;
}

void Texture::GenerateMipmaps() const {
	PTGN_ASSERT(IsBound(), "Texture must be bound prior to generating mipmaps for it");
#ifndef __EMSCRIPTEN__
	PTGN_ASSERT(
		ValidMinifyingForMipmaps(
			static_cast<TextureScaling>(GetParameterI(TextureParameter::MinifyingScaling))
		),
		"Set texture minifying scaling to mipmap type before generating mipmaps"
	);
#endif
	GLCall(GenerateMipmap(static_cast<GLenum>(TextureTarget::Texture2D)));
}

void Texture::Unbind(std::uint32_t slot) {
	SetActiveSlot(slot);
	BindId(0);
}

void Texture::BindId(TextureId id) {
	/*PTGN_LOG("GL: Bound texture with id ", id, " to slot: ", GetActiveSlot());*/
	GLCall(glBindTexture(static_cast<GLenum>(TextureTarget::Texture2D), id));
#ifdef PTGN_DEBUG
	++game.debug.stats.texture_binds;
#endif
#ifdef GL_ANNOUNCE_TEXTURE_CALLS
	PTGN_LOG("GL: Bound texture with id ", id);
#endif
}

void Texture::Bind(TextureId id, std::uint32_t slot) {
	SetActiveSlot(slot);
	BindId(id);
}

void Texture::Bind(std::uint32_t slot) const {
	Bind(id_, slot);
}

void Texture::Bind() const {
	PTGN_ASSERT(IsValid(), "Cannot bind destroyed or uninitialized texture");
	BindId(id_);
}

void Texture::SetActiveSlot(std::uint32_t slot) {
	PTGN_ASSERT(
		slot < GLRenderer::GetMaxTextureSlots(),
		"Attempting to bind a slot outside of OpenGL texture slot maximum"
	);
	GLCall(ActiveTexture(GL_TEXTURE0 + slot));
#ifdef GL_ANNOUNCE_TEXTURE_CALLS
	PTGN_LOG("GL: Set active texture slot to ", slot);
#endif
}

TextureId Texture::GetBoundId() {
	std::int32_t id{ -1 };
	GLCall(glGetIntegerv(static_cast<GLenum>(impl::GLBinding::Texture2D), &id));
	PTGN_ASSERT(id >= 0, "Failed to retrieve bound texture id");
	return static_cast<TextureId>(id);
}

bool Texture::IsBound() const {
	return GetBoundId() == id_;
}

bool Texture::IsValid() const {
	return id_;
}

TextureId Texture::GetId() const {
	return id_;
}

std::uint32_t Texture::GetActiveSlot() {
	std::int32_t id{ -1 };
	GLCall(glGetIntegerv(static_cast<GLenum>(impl::GLBinding::ActiveUnit), &id));
	PTGN_ASSERT(id >= GL_TEXTURE0, "Failed to retrieve the currently active texture slot");
	return static_cast<std::uint32_t>(id - GL_TEXTURE0);
}

bool Texture::ValidMinifyingForMipmaps(TextureScaling minifying) {
	return minifying == TextureScaling::LinearMipmapLinear ||
		   minifying == TextureScaling::LinearMipmapNearest ||
		   minifying == TextureScaling::NearestMipmapLinear ||
		   minifying == TextureScaling::NearestMipmapNearest;
}

void Texture::Resize(const V2_int& new_size) {
	TextureId restore_texture_id{ Texture::GetBoundId() };
	Bind();
	SetData(nullptr, new_size, GetFormat(), 0);
	Texture::BindId(restore_texture_id);
}

void Texture::SetData(
	const void* pixel_data, const V2_int& size, TextureFormat format, int mipmap_level
) {
	PTGN_ASSERT(IsBound(), "Texture must be bound prior to setting its data");

	PTGN_ASSERT(
		format != TextureFormat::Unknown, "Cannot set data for texture with unknown texture format"
	);

	auto formats{ GetGLFormats(format) };

	constexpr std::int32_t border{ 0 };

	GLType type{ GLType::UnsignedByte };

	if (formats.internal_format == InternalGLFormat::HDR_RGBA ||
		formats.internal_format == InternalGLFormat::HDR_RGB) {
		type = GLType::Float;
	} else if (formats.internal_format == InternalGLFormat::DEPTH24_STENCIL8) {
		type = GLType::UnsignedInt24_8;
	} else if (formats.internal_format == InternalGLFormat::DEPTH24) {
		type = GLType::Int;
	}

	PTGN_ASSERT(formats.internal_format != InternalGLFormat::STENCIL8);

	GLCall(glTexImage2D(
		static_cast<GLenum>(TextureTarget::Texture2D), mipmap_level,
		static_cast<GLint>(formats.internal_format), size.x, size.y, border,
		static_cast<GLenum>(formats.input_format), static_cast<GLenum>(type), pixel_data
	));

	size_	= size;
	format_ = format;
}

void Texture::SetSubData(
	const void* pixel_data, const V2_int& size, int mipmap_level, const V2_int& offset
) {
	PTGN_ASSERT(IsBound(), "Texture must be bound prior to setting its subdata");

	PTGN_ASSERT(pixel_data != nullptr);

	auto formats{ GetGLFormats(format_) };

	GLCall(glTexSubImage2D(
		static_cast<GLenum>(TextureTarget::Texture2D), mipmap_level, offset.x, offset.y, size.x,
		size.y, static_cast<GLenum>(formats.input_format),
		static_cast<GLenum>(impl::GLType::UnsignedByte), pixel_data
	));
}

V2_int TextureManager::GetSize(const TextureHandle& key) const {
	return Get(key).GetSize();
}

const Texture& TextureManager::Get(const TextureHandle& key) const {
	return ParentManager::Get(key);
}

Texture TextureManager::LoadFromFile(const path& filepath) {
	Surface s{ filepath };
	// For debugging: Print corners of the texture.
	/*Color ctl{ s.GetPixel({ 0, 0 }) };
	Color ctr{ s.GetPixel({ s.size.x - 1, 0 }) };
	Color cbr{ s.GetPixel({ s.size.x - 1, s.size.y - 1 }) };
	Color cbl{ s.GetPixel({ 0, s.size.y - 1 }) };
	PTGN_LOG("File: ", filepath, " | tl: ", ctl, " | tr: ", ctr, " | br: ", cbr, " | bl: ", cbl);*/
	return Texture{ s };
}

Color Surface::GetPixel(const V2_int& coordinate) const {
	PTGN_ASSERT(coordinate.x >= 0, "X Coordinate outside of range of grid");
	PTGN_ASSERT(coordinate.y >= 0, "Y Coordinate outside of range of grid");
	PTGN_ASSERT(coordinate.x < size.x, "X Coordinate outside of range of grid");
	PTGN_ASSERT(coordinate.y < size.y, "Y Coordinate outside of range of grid");
	auto index{ (static_cast<std::size_t>(coordinate.y) * static_cast<std::size_t>(size.x) +
				 static_cast<std::size_t>(coordinate.x)) *
				bytes_per_pixel };
	return GetPixel(index);
}

Color Surface::GetPixel(std::size_t index) const {
	PTGN_ASSERT(!data.empty(), "Cannot get pixel of an empty surface");
	PTGN_ASSERT(index < data.size(), "Coordinate outside of range of grid");
	index *= bytes_per_pixel;
	if (bytes_per_pixel == 4) {
		PTGN_ASSERT(index + 3 < data.size(), "Coordinate outside of range of grid");
		return { data[index + 0], data[index + 1], data[index + 2], data[index + 3] };
	} else if (bytes_per_pixel == 3) {
		PTGN_ASSERT(index + 2 < data.size(), "Coordinate outside of range of grid");
		return { data[index + 0], data[index + 1], data[index + 2], 255 };
	} else if (bytes_per_pixel == 1) {
		PTGN_ASSERT(index < data.size(), "Coordinate outside of range of grid");
		return { 255, 255, 255, data[index] };
	} else {
		PTGN_ERROR("Unsupported texture format");
	}
}

Surface::Surface(SDL_Surface* sdl_surface) {
	PTGN_ASSERT(sdl_surface != nullptr, "Invalid surface");

	format = TextureFormat::RGBA8888;

	// TODO: In the future, instead of converting all formats to RGBA, figure out how to deal with
	// Windows and MacOS discrepencies between image formats and SDL2 surface formats to enable the
	// use of RGB888 format (faster for JPGs). When I was using this approach in the past, MacOS had
	// an issue rendering JPG images as it perceived them as having 4 bytes per pixel with BGRA8888
	// format even though SDL2 said they were RGB888. Whereas on Windows, the same JPGs opened as 3
	// channel RGB888 surfaces as expected.
	SDL_Surface* surface = SDL_ConvertSurfaceFormat(
		sdl_surface, static_cast<std::uint32_t>(SDL_PIXELFORMAT_RGBA32), 0
	);

	PTGN_ASSERT(surface != nullptr, SDL_GetError());
	bytes_per_pixel = surface->format->BytesPerPixel;

	int lock{ SDL_LockSurface(surface) };
	PTGN_ASSERT(lock == 0, "Failed to lock surface when copying pixels");

	size = { surface->w, surface->h };

	std::size_t total_pixels{ static_cast<std::size_t>(size.x) * static_cast<std::size_t>(size.y) *
							  bytes_per_pixel };

	data.reserve(total_pixels);

	for (int y{ 0 }; y < size.y; ++y) {
		auto row{ static_cast<std::uint8_t*>(surface->pixels) + y * surface->pitch };
		for (int x{ 0 }; x < size.x; ++x) {
			auto pixel{ row + static_cast<std::size_t>(x) * bytes_per_pixel };
			for (std::size_t b{ 0 }; b < bytes_per_pixel; ++b) {
				data.push_back(pixel[b]);
			}
		}
	}

	SDL_UnlockSurface(surface);

	if (surface != sdl_surface) {
		// Surface was converted so new surface must be freed.
		SDL_FreeSurface(surface);
	}
	SDL_FreeSurface(sdl_surface);
}

Surface::Surface(const path& filepath) :
	Surface{ std::invoke([&]() {
		PTGN_ASSERT(
			FileExists(filepath),
			"Cannot create texture from a nonexistent filepath: ", filepath.string()
		);
		// Freed by Surface constructor.
		SDL_Surface* sdl_surface{ IMG_Load(filepath.string().c_str()) };
		PTGN_ASSERT(sdl_surface != nullptr, IMG_GetError());
		return sdl_surface;
	}) } {}

void Surface::FlipVertically() {
	PTGN_ASSERT(!data.empty(), "Cannot vertically flip an empty surface");
	// TODO: Check that this works as intended (i.e. middle row in odd height images is skipped).
	for (int row{ 0 }; row < size.y / 2; ++row) {
		std::swap_ranges(
			data.begin() + row * size.x, data.begin() + (row + 1) * size.x,
			data.begin() + (size.y - row - 1) * size.x
		);
	}
}

void Surface::ForEachPixel(const std::function<void(const V2_int&, const Color&)>& function) const {
	PTGN_ASSERT(!data.empty(), "Cannot loop through each pixel of an empty surface");
	PTGN_ASSERT(function != nullptr, "Invalid loop function");
	for (int j{ 0 }; j < size.y; j++) {
		auto idx_row{ static_cast<std::size_t>(j) * static_cast<std::size_t>(size.x) };
		for (int i{ 0 }; i < size.x; i++) {
			V2_int coordinate{ i, j };
			auto index{ idx_row + static_cast<std::size_t>(i) };
			function(coordinate, GetPixel(index));
		}
	}
}

void Texture::SetClampBorderColor(const Color& color) const {
	PTGN_ASSERT(IsValid(), "Cannot set clamp border color of invalid or uninitialized texture");

	TextureId restore_id{ Texture::GetBoundId() };

	Bind();

	V4_float c{ color.Normalized() };
	std::array<float, 4> border_color{ c.x, c.y, c.z, c.w };

	GLCall(glTexParameterfv(
		static_cast<GLenum>(TextureTarget::Texture2D),
		static_cast<GLenum>(impl::TextureParameter::BorderColor), border_color.data()
	));

	Texture::BindId(restore_id);
}

} // namespace impl

const impl::Texture& TextureHandle::GetTexture(const Entity& entity) const {
	if (TextureHandle::GetHash()) {
		PTGN_ASSERT(
			game.texture.Has(*this), "Texture must be loaded into the texture manager: ", key_
		);
		return game.texture.Get(*this);
	}
	PTGN_ASSERT(entity, "Texture must be owned by a valid entity");
	if (entity.Has<impl::Texture>()) {
		return entity.Get<impl::Texture>();
	}
	if (entity.Has<impl::FrameBuffer>()) {
		return entity.Get<impl::FrameBuffer>().GetTexture();
	}
	PTGN_ERROR("Entity does not have a valid texture handle");
}

impl::Texture& TextureHandle::GetTexture(const Entity& entity) {
	return const_cast<impl::Texture&>(std::as_const(*this).GetTexture(entity));
}

V2_int TextureHandle::GetSize(const Entity& entity) const {
	return GetTexture(entity).GetSize();
}

} // namespace ptgn