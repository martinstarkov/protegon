#include "renderer/texture.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "core/game.h"
#include "math/hash.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/flip.h"
#include "renderer/gl_helper.h"
#include "renderer/gl_loader.h"
#include "renderer/gl_renderer.h"
#include "renderer/gl_types.h"
#include "SDL_error.h"
#include "SDL_image.h"
#include "SDL_pixels.h"
#include "SDL_surface.h"
#include "serialization/json.h"
#include "utility/assert.h"
#include "utility/file.h"
#include "utility/log.h"
#include "utility/stats.h"

namespace ptgn {

Color GetPixel(const path& texture_filepath, const V2_int& coordinate) {
	impl::Surface s{ texture_filepath };
	return s.GetPixel(coordinate);
}

void ForEachPixel(
	const path& texture_filepath, const std::function<void(const V2_int&, const Color&)>& function
) {
	impl::Surface s{ texture_filepath };
	return s.ForEachPixel(function);
}

namespace impl {

TextureFormat GetFormatFromOpenGL(InternalGLFormat opengl_internal_format) {
	switch (opengl_internal_format) {
		case InternalGLFormat::RGBA8: return TextureFormat::RGBA8888;
		case InternalGLFormat::RGB8:  return TextureFormat::RGB888;
		case InternalGLFormat::R8:	  return TextureFormat::A8;
		default:					  PTGN_ERROR("Unsupported or unrecognized OpenGL format");
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
	Bind();
	SetData(data, size, format, mipmap_level);
	SetParameterI(TextureParameter::WrapS, static_cast<int>(wrapping_x));
	SetParameterI(TextureParameter::WrapT, static_cast<int>(wrapping_y));
	SetParameterI(TextureParameter::MinifyingScaling, static_cast<int>(minifying));
	SetParameterI(TextureParameter::MagnifyingScaling, static_cast<int>(magnifying));
	if (mipmaps) {
		GenerateMipmaps();
	}
}

Texture::Texture(Texture&& other) noexcept :
	id_{ std::exchange(other.id_, 0) }, size_{ std::exchange(other.size_, {}) } {}

Texture& Texture::operator=(Texture&& other) noexcept {
	if (this != &other) {
		DeleteTexture();
		id_	  = std::exchange(other.id_, 0);
		size_ = std::exchange(other.size_, {});
	}
	return *this;
}

Texture::~Texture() {
	DeleteTexture();
}

bool Texture::operator==(const Texture& other) const {
	return id_ == other.id_;
}

bool Texture::operator!=(const Texture& other) const {
	return !(*this == other);
}

void Texture::GenerateTexture() {
	GLCall(gl::glGenTextures(1, &id_));
	PTGN_ASSERT(IsValid(), "Failed to generate texture using OpenGL context");
#ifdef GL_ANNOUNCE_TEXTURE_CALLS
	PTGN_LOG("GL: Generated texture with id ", id_);
#endif
}

void Texture::DeleteTexture() noexcept {
	if (!IsValid()) {
		return;
	}
	GLCall(gl::glDeleteTextures(1, &id_));
#ifdef GL_ANNOUNCE_TEXTURE_CALLS
	PTGN_LOG("GL: Deleted texture with id ", id_);
#endif
	id_ = 0;
}

TextureFormat Texture::GetFormat() const {
	return GetFormatFromOpenGL(
		static_cast<InternalGLFormat>(GetLevelParameterI(TextureLevelParameter::InternalFormat, 0))
	);
}

V2_int Texture::GetSize() const {
	return size_;
}

void Texture::SetParameterI(TextureParameter parameter, std::int32_t value) const {
	PTGN_ASSERT(IsBound(), "Texture must be bound prior to setting its parameters");
	PTGN_ASSERT(value != -1, "Cannot set texture parameter value to -1");
	GLCall(gl::glTexParameteri(
		static_cast<gl::GLenum>(TextureTarget::Texture2D), static_cast<gl::GLenum>(parameter), value
	));
}

std::int32_t Texture::GetParameterI(TextureParameter parameter) const {
	PTGN_ASSERT(IsBound(), "Texture must be bound prior to getting its parameters");
	std::int32_t value{ -1 };
	GLCall(gl::glGetTexParameteriv(
		static_cast<gl::GLenum>(TextureTarget::Texture2D), static_cast<gl::GLenum>(parameter),
		&value
	));
	PTGN_ASSERT(value != -1, "Failed to retrieve texture parameter");
	return value;
}

std::int32_t Texture::GetLevelParameterI(TextureLevelParameter parameter, std::int32_t level)
	const {
	PTGN_ASSERT(IsBound(), "Texture must be bound prior to getting its level parameters");
	std::int32_t value{ -1 };
	GLCall(gl::glGetTexLevelParameteriv(
		static_cast<gl::GLenum>(TextureTarget::Texture2D), level,
		static_cast<gl::GLenum>(parameter), &value
	));
	PTGN_ASSERT(value != -1, "Failed to retrieve texture level parameter");
	return value;
}

void Texture::GenerateMipmaps() const {
	PTGN_ASSERT(IsBound(), "Texture must be bound prior to generating mipmaps for it");
	PTGN_ASSERT(
		ValidMinifyingForMipmaps(
			static_cast<TextureScaling>(GetParameterI(TextureParameter::MinifyingScaling))
		),
		"Set texture minifying scaling to mipmap type before generating mipmaps"
	);
	GLCall(gl::GenerateMipmap(static_cast<gl::GLenum>(TextureTarget::Texture2D)));
}

void Texture::Unbind(std::uint32_t slot) {
	SetActiveSlot(slot);
	BindId(0);
}

void Texture::BindId(std::uint32_t id) {
	GLCall(gl::glBindTexture(static_cast<gl::GLenum>(TextureTarget::Texture2D), id));
#ifdef PTGN_DEBUG
	++game.stats.texture_binds;
#endif
#ifdef GL_ANNOUNCE_TEXTURE_CALLS
	PTGN_LOG("GL: Bound texture with id ", id);
#endif
}

void Texture::Bind(std::uint32_t id, std::uint32_t slot) {
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
	GLCall(gl::ActiveTexture(GL_TEXTURE0 + slot));
#ifdef GL_ANNOUNCE_TEXTURE_CALLS
	PTGN_LOG("GL: Set active texture slot to ", slot);
#endif
}

std::uint32_t Texture::GetBoundId() {
	std::int32_t id{ -1 };
	GLCall(gl::glGetIntegerv(static_cast<gl::GLenum>(impl::GLBinding::Texture2D), &id));
	PTGN_ASSERT(id >= 0, "Failed to retrieve bound texture id");
	return static_cast<std::uint32_t>(id);
}

bool Texture::IsBound() const {
	return GetBoundId() == id_;
}

bool Texture::IsValid() const {
	return id_;
}

std::uint32_t Texture::GetId() const {
	return id_;
}

std::uint32_t Texture::GetActiveSlot() {
	std::int32_t id{ -1 };
	GLCall(gl::glGetIntegerv(static_cast<gl::GLenum>(impl::GLBinding::ActiveTexture), &id));
	PTGN_ASSERT(id >= 0, "Failed to retrieve the currently active texture slot");
	return static_cast<std::uint32_t>(id);
}

bool Texture::ValidMinifyingForMipmaps(TextureScaling minifying) {
	return minifying == TextureScaling::LinearMipmapLinear ||
		   minifying == TextureScaling::LinearMipmapNearest ||
		   minifying == TextureScaling::NearestMipmapLinear ||
		   minifying == TextureScaling::NearestMipmapNearest;
}

void Texture::Resize(const V2_int& new_size) {
	Bind();
	SetData(nullptr, new_size, GetFormat(), 0);
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

	GLCall(gl::glTexImage2D(
		static_cast<gl::GLenum>(TextureTarget::Texture2D), mipmap_level,
		static_cast<gl::GLint>(formats.internal_format), size.x, size.y, border,
		static_cast<gl::GLenum>(formats.input_format),
		static_cast<gl::GLenum>(GLType::UnsignedByte), pixel_data
	));

	size_ = size;
}

void Texture::SetSubData(
	const void* pixel_data, const V2_int& size, TextureFormat format, int mipmap_level,
	const V2_int& offset
) {
	PTGN_ASSERT(IsBound(), "Texture must be bound prior to setting its subdata");

	PTGN_ASSERT(pixel_data != nullptr);

	auto formats{ GetGLFormats(format) };

	GLCall(gl::glTexSubImage2D(
		static_cast<gl::GLenum>(TextureTarget::Texture2D), mipmap_level, offset.x, offset.y, size.x,
		size.y, static_cast<gl::GLenum>(formats.input_format),
		static_cast<gl::GLenum>(impl::GLType::UnsignedByte), pixel_data
	));
}

void TextureManager::Load(const path& json_filepath) {
	auto textures{ LoadJson(json_filepath) };
	for (auto& [texture_key, texture_path] : textures.items()) {
		Load(texture_key, texture_path);
	}
}

void TextureManager::Load(std::string_view key, const path& filepath) {
	auto [it, inserted] = textures_.try_emplace(Hash(key));
	if (inserted) {
		it->second = LoadFromFile(filepath);
	}
}

void TextureManager::Unload(std::string_view key) {
	textures_.erase(Hash(key));
}

V2_int TextureManager::GetSize(std::string_view key) const {
	PTGN_ASSERT(Has(key), "Cannot get size of texture which has not been loaded");
	return textures_.find(Hash(key))->second.GetSize();
}

bool TextureManager::Has(std::string_view key) const {
	return Has(Hash(key));
}

const Texture& TextureManager::Get(std::size_t key) const {
	PTGN_ASSERT(Has(key), "Cannot get texture which has not been loaded");
	return textures_.find(key)->second;
}

bool TextureManager::Has(std::size_t key) const {
	return textures_.find(key) != textures_.end();
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
	auto index{ (static_cast<std::size_t>(coordinate.y) * size.x + coordinate.x) *
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

	SDL_Surface* surface{ sdl_surface };

	format			= GetFormatFromSDL(surface->format->format);
	bytes_per_pixel = surface->format->BytesPerPixel;

	auto convert_to_format = [&](TextureFormat f) {
		format			= f;
		surface			= SDL_ConvertSurfaceFormat(surface, static_cast<std::uint32_t>(format), 0);
		bytes_per_pixel = surface->format->BytesPerPixel;
		PTGN_ASSERT(surface != nullptr, SDL_GetError());
	};

	if (format == TextureFormat::Unknown) {
		// Convert format to RGBA8888.
		std::invoke(convert_to_format, TextureFormat::RGBA8888);
	} else if (format == TextureFormat::A8) {
		std::invoke(convert_to_format, TextureFormat::ABGR8888);
	}

	int lock{ SDL_LockSurface(surface) };
	PTGN_ASSERT(lock == 0, "Failed to lock surface when copying pixels");

	size = { surface->w, surface->h };

	std::size_t total_pixels{ static_cast<std::size_t>(size.x) * size.y * bytes_per_pixel };

	data.reserve(total_pixels);

	for (int y = 0; y < size.y; ++y) {
		auto row = static_cast<std::uint8_t*>(surface->pixels) + y * surface->pitch;
		for (int x = 0; x < size.x; ++x) {
			auto pixel = row + x * bytes_per_pixel;
			for (int b = 0; b < bytes_per_pixel; ++b) {
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
		auto idx_row{ static_cast<std::size_t>(j) * size.x };
		for (int i{ 0 }; i < size.x; i++) {
			V2_int coordinate{ i, j };
			auto index{ idx_row + i };
			std::invoke(function, coordinate, GetPixel(index));
		}
	}
}

/*
// TODO: Include in texture:
void Texture::SetClampBorderColor(const Color& color) const {
	PTGN_ASSERT(IsValid(), "Cannot set clamp border color of invalid or uninitialized texture");

	std::uint32_t restore_id{ Texture::GetBoundId() };

	Bind();

	V4_float c{ color.Normalized() };
	std::array<float, 4> border_color{ c.x, c.y, c.z, c.w };

	GLCall(gl::glTexParameterfv(
		static_cast<gl::GLenum>(TextureTarget::Texture2D),
		static_cast<gl::GLenum>(impl::TextureParameter::BorderColor), border_color.data()
	));

	Texture::BindId(restore_id);
}
*/

} // namespace impl

} // namespace ptgn