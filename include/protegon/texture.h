#pragma once

#include <cstdint>

#include "protegon/file.h"
#include "protegon/surface.h"
#include "protegon/vector2.h"
#include "utility/handle.h"

namespace ptgn {

enum class Flip {
	// Source: https://wiki.libsdl.org/SDL2/SDL_RendererFlip

	None	   = 0x00000000,
	Horizontal = 0x00000001,
	Vertical   = 0x00000002
};

enum class TextureWrapping {
	ClampEdge	   = 0x812F, // GL_CLAMP_TO_EDGE,
	ClampBorder	   = 0x812D, // GL_CLAMP_TO_BORDER,
	Repeat		   = 0x2901, // GL_REPEAT,
	MirroredRepeat = 0x8370	 // GL_MIRRORED_REPEAT
};

enum class TextureFilter {
	Nearest				 = 0x2600, // GL_NEAREST
	Linear				 = 0x2601, // GL_LINEAR
	NearestMipmapNearest = 0x2700, // GL_NEAREST_MIPMAP_NEAREST
	NearestMipmapLinear	 = 0x2702, // GL_NEAREST_MIPMAP_LINEAR
	LinearMipmapNearest	 = 0x2701, // GL_LINEAR_MIPMAP_NEAREST
	LinearMipmapLinear	 = 0x2703, // GL_LINEAR_MIPMAP_LINEAR
};

class Renderer;

namespace impl {

class RendererData;

struct GLFormats {
	// first
	std::int32_t internal_{ 0 };
	// second
	std::uint32_t format_{ 0 };
};

struct TextureInstance {
	TextureInstance();
	~TextureInstance();

	std::uint32_t id_{ 0 };
	V2_int size_;
};

} // namespace impl

class Texture : public Handle<impl::TextureInstance> {
public:
	Texture()  = default;
	~Texture() = default;

private:
	constexpr const static TextureFilter default_minifying_filter{ TextureFilter::Nearest };
	constexpr const static TextureFilter default_magnifying_filter{ TextureFilter::Nearest };
	constexpr const static TextureWrapping default_wrapping{ TextureWrapping::ClampEdge };

public:
	Texture(const path& image_path, ImageFormat format = ImageFormat::RGBA8888);
	Texture(const Surface& surface);
	Texture(const void* pixel_data, const V2_int& size, ImageFormat format);
	Texture(const std::vector<Color>& pixels, const V2_int& size);

	void SetWrapping(TextureWrapping s);
	void SetWrapping(TextureWrapping s, TextureWrapping t);
	void SetWrapping(TextureWrapping s, TextureWrapping t, TextureWrapping r);

	void SetFilters(TextureFilter minifying, TextureFilter magnifying);
	void SetBorderColor(const Color& color);

	void GenerateMipmaps();

	void SetSubData(const void* pixel_data, ImageFormat format);
	void SetSubData(const std::vector<Color>& pixels);

	bool operator==(const Texture& o) const;
	bool operator!=(const Texture& o) const;

	V2_int GetSize() const;

	void Bind() const;
	void Bind(std::uint32_t slot) const;

private:
	friend class impl::RendererData;
	friend class Renderer;

	static std::int32_t BoundId();

	// static void Unbind();

	void SetDataImpl(const void* pixel_data, const V2_int& size, ImageFormat format);
};

} // namespace ptgn