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

#include "SDL_image.h"
#include "SDL_pixels.h"
#include "SDL_surface.h"
#include "core/game.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/flip.h"
#include "renderer/gl_helper.h"
#include "renderer/gl_loader.h"
#include "renderer/gl_renderer.h"
#include "renderer/gl_types.h"
#include "utility/debug.h"
#include "utility/file.h"
#include "utility/log.h"
#include "utility/stats.h"

namespace ptgn::impl {

TextureFormat GetFormatFromSDL(std::uint32_t sdl_format) {
	switch (sdl_format) {
		case SDL_PIXELFORMAT_RGBA32:   [[fallthrough]];
		case SDL_PIXELFORMAT_RGBA8888: return TextureFormat::RGBA8888;
		case SDL_PIXELFORMAT_RGB24:	   [[fallthrough]];
		case SDL_PIXELFORMAT_RGB888:   return TextureFormat::RGB888;
		case SDL_PIXELFORMAT_BGRA32:   [[fallthrough]];
		case SDL_PIXELFORMAT_BGRA8888: return TextureFormat::BGRA8888;
		case SDL_PIXELFORMAT_BGR24:	   [[fallthrough]];
		case SDL_PIXELFORMAT_BGR888:   return TextureFormat::BGR888;
		case SDL_PIXELFORMAT_INDEX8:   [[fallthrough]];
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

void TextureManager::SetParameterI(TextureParameter parameter, std::int32_t value) {
	PTGN_ASSERT(value != -1, "Cannot set texture parameter value to -1");
	GLCall(gl::glTexParameteri(
		static_cast<gl::GLenum>(TextureTarget::Texture2D), static_cast<gl::GLenum>(parameter), value
	));
}

std::int32_t TextureManager::GetParameterI(TextureParameter parameter) {
	std::int32_t value{ -1 };
	GLCall(gl::glGetTexParameteriv(
		static_cast<gl::GLenum>(TextureTarget::Texture2D), static_cast<gl::GLenum>(parameter),
		&value
	));
	PTGN_ASSERT(value != -1, "Failed to retrieve texture parameter");
	return value;
}

std::int32_t TextureManager::GetLevelParameterI(
	TextureLevelParameter parameter, std::int32_t level
) {
	std::int32_t value{ -1 };
	GLCall(gl::glGetTexLevelParameteriv(
		static_cast<gl::GLenum>(TextureTarget::Texture2D), level,
		static_cast<gl::GLenum>(parameter), &value
	));
	PTGN_ASSERT(value != -1, "Failed to retrieve texture level parameter");
	return value;
}

void TextureManager::GenerateMipmaps() {
	PTGN_ASSERT(
		ValidMinifyingForMipmaps(
			static_cast<TextureScaling>(GetParameterI(TextureParameter::MinifyingScaling))
		),
		"Set texture minifying scaling to mipmap type before generating mipmaps"
	);
	GLCall(gl::GenerateMipmap(static_cast<gl::GLenum>(TextureTarget::Texture2D)));
}

void TextureManager::Unbind(std::uint32_t slot) {
	SetActiveSlot(slot);
	Bind(0);
}

void TextureManager::Bind(std::uint32_t id) {
#ifdef GL_ANNOUNCE_TEXTURE_CALLS
	PTGN_LOG("GL: Bound texture with id ", id);
#endif
	GLCall(gl::glBindTexture(static_cast<gl::GLenum>(TextureTarget::Texture2D), id));
#ifdef PTGN_DEBUG
	++game.stats.texture_binds;
#endif
}

void TextureManager::Bind(std::uint32_t id, std::uint32_t slot) {
	SetActiveSlot(slot);
	Bind(id);
}

void TextureManager::SetActiveSlot(std::uint32_t slot) {
	PTGN_ASSERT(
		slot < GLRenderer::GetMaxTextureSlots(),
		"Attempting to bind a slot outside of OpenGL texture slot maximum"
	);
#ifdef GL_ANNOUNCE_TEXTURE_CALLS
	PTGN_LOG("GL: Set active texture slot to ", slot);
#endif
	GLCall(gl::ActiveTexture(GL_TEXTURE0 + slot));
}

std::uint32_t TextureManager::GetBoundId() {
	std::int32_t id{ -1 };
	GLCall(gl::glGetIntegerv(static_cast<gl::GLenum>(impl::GLBinding::Texture2D), &id));
	PTGN_ASSERT(id >= 0, "Failed to retrieve bound texture id");
	return static_cast<std::uint32_t>(id);
}

std::uint32_t TextureManager::GetActiveSlot() {
	std::int32_t id{ -1 };
	GLCall(gl::glGetIntegerv(static_cast<gl::GLenum>(impl::GLBinding::ActiveTexture), &id));
	PTGN_ASSERT(id >= 0, "Failed to retrieve the currently active texture slot");
	return static_cast<std::uint32_t>(id);
}

bool TextureManager::ValidMinifyingForMipmaps(TextureScaling minifying) {
	return minifying == TextureScaling::LinearMipmapLinear ||
		   minifying == TextureScaling::LinearMipmapNearest ||
		   minifying == TextureScaling::NearestMipmapLinear ||
		   minifying == TextureScaling::NearestMipmapNearest;
}

void TextureManager::SetData(
	const void* pixel_data, TextureFormat format, const V2_int& size, int mipmap_level
) {
	PTGN_ASSERT(
		format != TextureFormat::Unknown, "Cannot set data for texture with unknown texture format"
	);

	auto formats{ GetGLFormats(format) };

	constexpr std::int32_t border{ 0 };

	GLCall(gl::glTexImage2D(
		static_cast<gl::GLenum>(TextureTarget::Texture2D), mipmap_level,
		static_cast<gl::GLint>(formats.internal_format), size.x, size.y, border,
		formats.input_format, static_cast<gl::GLenum>(GLType::UnsignedByte), pixel_data
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

void TextureManager::SetSubData(
	const void* pixel_data, TextureFormat format, const V2_int& size, int mipmap_level,
	const V2_int& offset
) {
	PTGN_ASSERT(pixel_data != nullptr);

	auto formats{ GetGLFormats(format) };

	GLCall(gl::glTexSubImage2D(
		static_cast<gl::GLenum>(TextureTarget::Texture2D), mipmap_level, offset.x, offset.y, size.x,
		size.y, formats.input_format, static_cast<gl::GLenum>(impl::GLType::UnsignedByte),
		pixel_data
	));
}

/*
void TextureManager::DrawToBoundFrameBuffer(
	std::string_view key, Rect destination, TextureInfo texture_info, const Shader& shader
) const {
	PTGN_ASSERT(shader.IsValid(), "Cannot draw texture with uninitialized or destroyed shader");

	if (destination.IsZero()) {
		// TODO: Change this to take into account the screen target viewport.
		destination = Rect::Fullscreen();
	} else if (destination.size.IsZero()) {
		destination.size = size_;
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

	auto tex_coords{ texture_info.GetTextureCoordinates(size_) };

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
*/

TextureManager::TextureInstance::TextureInstance() {
	GLCall(gl::glGenTextures(1, &id));
	PTGN_ASSERT(id != 0, "Failed to generate texture using OpenGL context");
#ifdef GL_ANNOUNCE_TEXTURE_CALLS
	PTGN_LOG("GL: Generated texture with id ", id);
#endif
}

TextureManager::TextureInstance::~TextureInstance() {
	GLCall(gl::glDeleteTextures(1, &id));
#ifdef GL_ANNOUNCE_TEXTURE_CALLS
	PTGN_LOG("GL: Deleted texture with id ", id);
#endif
}

void TextureManager::TextureInstance::Setup(
	const void* data, TextureFormat format, const V2_int& size, int mipmap_level,
	TextureWrapping wrapping_x, TextureWrapping wrapping_y, TextureScaling minifying,
	TextureScaling magnifying, bool mipmaps
) {
	TextureManager::Bind(id);
	TextureManager::SetData(data, format, size, mipmap_level);
	TextureManager::SetParameterI(TextureParameter::WrapS, static_cast<int>(wrapping_x));
	TextureManager::SetParameterI(TextureParameter::WrapT, static_cast<int>(wrapping_y));
	TextureManager::SetParameterI(TextureParameter::MinifyingScaling, static_cast<int>(minifying));
	TextureManager::SetParameterI(
		TextureParameter::MagnifyingScaling, static_cast<int>(magnifying)
	);
	if (mipmaps) {
		TextureManager::GenerateMipmaps();
	}
	this->size = size;
}

void TextureManager::Load(std::string_view key, const path& filepath) {
	auto [it, inserted] = textures_.try_emplace(Hash(key));
	if (inserted) {
		Surface s{ LoadFromFile(filepath) };
		it->second->Setup(
			static_cast<const void*>(s.data.data()), s.format, s.size, 0, default_wrapping,
			default_wrapping, default_minifying_scaling, default_magnifying_scaling, true
		);
	}
}

void TextureManager::Unload(std::string_view key) {
	textures_.erase(Hash(key));
}

V2_int TextureManager::GetSize(std::string_view key) const {
	PTGN_ASSERT(Has(Hash(key)), "Cannot get size of texture which has not been loaded");
	return textures_.find(Hash(key))->second->size;
}

bool TextureManager::Has(std::size_t key) const {
	return textures_.find(key) != textures_.end();
}

Surface TextureManager::LoadFromFile(const path& filepath) {
	PTGN_ASSERT(
		FileExists(filepath),
		"Cannot create texture from a nonexistent filepath: ", filepath.string()
	);

	SDL_Surface* sdl_surface{ IMG_Load(filepath.string().c_str()) };
	PTGN_ASSERT(sdl_surface != nullptr, IMG_GetError());

	Surface surface{ sdl_surface };

	SDL_FreeSurface(sdl_surface);

	return surface;
}

Surface::Surface(SDL_Surface* sdl_surface) {
	PTGN_ASSERT(sdl_surface != nullptr, "Invalid surface");

	SDL_Surface* surface{ sdl_surface };

	format = GetFormatFromSDL(surface->format->format);

	if (format == TextureFormat::Unknown) {
		// Convert format.
		format	= TextureFormat::RGBA8888;
		surface = SDL_ConvertSurfaceFormat(surface, static_cast<std::uint32_t>(format), 0);
		PTGN_ASSERT(surface != nullptr, SDL_GetError());
	}

	PTGN_ASSERT(
		format != TextureFormat::Unknown, "Cannot create surface with unknown texture format"
	);

	int lock{ SDL_LockSurface(surface) };
	PTGN_ASSERT(lock == 0, "Failed to lock surface when copying pixels");

	size = { surface->w, surface->h };

	std::size_t total_pixels{ static_cast<std::size_t>(size.x) * size.y };

	data.resize(total_pixels, color::Transparent);

	bool r_mask{ surface->format->Rmask == 0x000000ff };
	bool b_mask{ surface->format->Bmask == 0x000000ff };

	auto get_color = [&](const std::uint8_t* pixel) {
		switch (surface->format->BytesPerPixel) {
			case 4: {
				if (r_mask) {
					return Color{ pixel[0], pixel[1], pixel[2], pixel[3] };
				} else if (b_mask) {
					return Color{ pixel[2], pixel[1], pixel[0], pixel[3] };
				} else {
					return Color{ pixel[3], pixel[2], pixel[1], pixel[0] };
				}
			}
			case 3: {
				if (r_mask) {
					return Color{ pixel[0], pixel[1], pixel[2], 255 };
				} else if (b_mask) {
					return Color{ pixel[2], pixel[1], pixel[0], 255 };
				} else {
					PTGN_ERROR("Failed to identify mask for fully opaque pixel format");
				}
			}
			case 1: {
				return Color{ 255, 255, 255, pixel[0] };
				break;
			}
			default: PTGN_ERROR("Unsupported texture format"); break;
		}
	};

	for (int y{ 0 }; y < size.y; ++y) {
		const std::uint8_t* row{ static_cast<std::uint8_t*>(surface->pixels) + y * surface->pitch };
		auto idx_row{ static_cast<std::size_t>(y) * size.x };
		for (int x{ 0 }; x < size.x; ++x) {
			const std::uint8_t* pixel{ row + x * surface->format->BytesPerPixel };
			auto index{ idx_row + x };
			PTGN_ASSERT(index < data.size());
			data[index] = std::invoke(get_color, pixel);
		}
	}

	PTGN_ASSERT(data.size() == total_pixels);

	SDL_UnlockSurface(surface);

	if (surface != sdl_surface) {
		// Surface was converted so new surface must be freed.
		SDL_FreeSurface(surface);
	}
}

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

Color Surface::GetPixel(const V2_int& coordinate) const {
	PTGN_ASSERT(!data.empty(), "Cannot get pixel of an empty surface");
	auto index{ static_cast<std::size_t>(coordinate.y) * size.x + coordinate.x };
	PTGN_ASSERT(coordinate.x >= 0, "Coordinate outside of range of grid");
	PTGN_ASSERT(coordinate.y >= 0, "Coordinate outside of range of grid");
	PTGN_ASSERT(index < data.size(), "Coordinate outside of range of grid");
	return data[index];
}

void Surface::ForEachPixel(const std::function<void(const V2_int&, const Color&)>& function) const {
	PTGN_ASSERT(!data.empty(), "Cannot loop through each pixel of an empty surface");
	for (int j{ 0 }; j < size.y; j++) {
		auto idx_row{ static_cast<std::size_t>(j) * size.x };
		for (int i{ 0 }; i < size.x; i++) {
			V2_int coordinate{ i, j };
			auto index{ idx_row + i };
			PTGN_ASSERT(index < data.size());
			std::invoke(function, coordinate, data[index]);
		}
	}
}

/*
void Texture::Draw(Rect destination, const TextureInfo& texture_info, std::int32_t render_layer)
	const {
	PTGN_ASSERT(IsValid(), "Cannot draw uninitialized or destroyed texture");

	if (destination.IsZero()) {
		// TODO: Change this to take into account window resolution.
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

void Texture::Draw(const Rect& destination, const TextureInfo& texture_info, const Shader& shader)
	const {
	game.renderer.Flush();

	PTGN_ASSERT(shader.IsValid(), "Cannot draw texture with uninitialized or destroyed shader");

	DrawToBoundFrameBuffer(destination, texture_info, shader);
}
}*/

/*
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

} // namespace ptgn::impl