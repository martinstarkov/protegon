#pragma once

#include <cstdint>
#include <variant>
#include <vector>

#include "core/manager.h"
#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/flip.h"
#include "renderer/layer_info.h"
#include "renderer/surface.h"
#include "utility/file.h"
#include "utility/handle.h"

namespace ptgn {

struct TextureInfo {
	TextureInfo() = default;

	TextureInfo(
		const V2_float& source_position, const V2_float& source_size, Flip flip = Flip::None,
		const Color& tint = color::White, const V2_float& rotation_center = { 0.5f, 0.5f }
	) :
		source_position{ source_position },
		source_size{ source_size },
		flip{ flip },
		tint{ tint },
		rotation_center{ rotation_center } {}

	/*
	source_position Top left pixel to start drawing texture from within the texture (defaults to {
	0, 0}).
	*/
	V2_float source_position;
	/*
	source.size Number of pixels of the texture to draw (defaults to {} which corresponds to the
	remaining texture size to the bottom right of source_position). source.origin Relative  to
	destination_position the direction from which the texture is.
	*/
	V2_float source_size;
	// Mirror the texture along an axis (default to Flip::None).
	Flip flip{ Flip::None };
	// Color to tint the texture. Allows to change the transparency of a texture. (default:
	// color::White corresponds to no tint effect ).
	Color tint{ color::White };
	// Fraction of the source_size around which the texture is rotated (defaults to{ 0.5f, 0.5f }
	// which corresponds to the center of the texture).
	V2_float rotation_center{ 0.5f, 0.5f };
};

enum class TextureWrapping {
	ClampEdge	   = 0x812F, // GL_CLAMP_TO_EDGE
	ClampBorder	   = 0x812D, // GL_CLAMP_TO_BORDER
	Repeat		   = 0x2901, // GL_REPEAT
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

namespace impl {

class Renderer;
class RendererData;
class TextureBatchData;

enum class InternalGLFormat {
	RGB8  = 0x8051, // GL_RGB8
	RGBA8 = 0x8058, // GL_RGBA8
};

enum class TextureParameter {
	BorderColor = 0x1004, // GL_TEXTURE_BORDER_COLOR
	Width		= 0x1000, // GL_TEXTURE_WIDTH
	Height		= 0x1001, // GL_TEXTURE_HEIGHT
	WrapS		= 0x2802, // GL_TEXTURE_WRAP_S
	WrapT		= 0x2803, // GL_TEXTURE_WRAP_T
	WrapR		= 0x8072, // GL_TEXTURE_WRAP_R
	MagFilter	= 0x2800, // GL_TEXTURE_MAG_FILTER
	MinFilter	= 0x2801, // GL_TEXTURE_MIN_FILTER
};

struct GLFormats {
	InternalGLFormat internal_{ InternalGLFormat::RGBA8 };
	std::uint32_t format_{ 0 };
	int components_{ 0 };
};

[[nodiscard]] GLFormats GetGLFormats(ImageFormat format);

struct TextureInstance {
	TextureInstance();
	~TextureInstance();

	std::uint32_t id_{ 0 };
	V2_int size_;
	ImageFormat format_{ ImageFormat::RGBA8888 };
};

} // namespace impl

class Texture : public Handle<impl::TextureInstance> {
public:
	Texture()			= default;
	~Texture() override = default;

private:
	constexpr const static TextureFilter default_minifying_filter{ TextureFilter::Nearest };
	constexpr const static TextureFilter default_magnifying_filter{ TextureFilter::Nearest };
	constexpr const static TextureWrapping default_wrapping{ TextureWrapping::ClampEdge };

public:
	constexpr const static ImageFormat default_format{ ImageFormat::RGBA8888 };

public:
	Texture(const path& image_path, ImageFormat format = default_format);
	explicit Texture(const Surface& surface);
	Texture(
		const void* pixel_data, const V2_int& size, ImageFormat format,
		TextureWrapping wrapping = default_wrapping,
		TextureFilter minifying	 = default_minifying_filter,
		TextureFilter magnifying = default_minifying_filter, bool mipmaps = true
	);
	Texture(const std::vector<Color>& pixels, const V2_int& size);

	// If destination.size is {}, fullscreen texture will be drawn.
	void Draw(
		const Rect& destination = {}, const TextureInfo& texture_info = {},
		const LayerInfo& layer_info = {}
	) const;

	void SetWrapping(TextureWrapping s) const;
	void SetWrapping(TextureWrapping s, TextureWrapping t) const;
	void SetWrapping(TextureWrapping s, TextureWrapping t, TextureWrapping r) const;

	void SetFilters(TextureFilter minifying, TextureFilter magnifying) const;

	// Sets the "out of bounds" texture color when using TextureWrapping::ClampBorder
	void SetClampBorderColor(const Color& color) const;

	void GenerateMipmaps() const;

	void SetSubData(const void* pixel_data, ImageFormat format);
	void SetSubData(const std::vector<Color>& pixels);

	[[nodiscard]] V2_int GetSize() const;
	[[nodiscard]] ImageFormat GetFormat() const;

	void Bind() const;
	void Bind(std::uint32_t slot) const;
	void SetActiveSlot(std::uint32_t slot) const;

private:
	friend class impl::TextureBatchData;
	friend class impl::RendererData;
	friend class Renderer;
	friend class FrameBuffer;

	[[nodiscard]] static std::int32_t GetBoundId();
	[[nodiscard]] static std::int32_t GetActiveSlot();

	void SetDataImpl(const void* pixel_data, const V2_int& size, ImageFormat format);
};

namespace impl {

class TextureManager : public MapManager<Texture> {
public:
	using MapManager::MapManager;
};

} // namespace impl

using TextureOrKey =
	std::variant<Texture, impl::TextureManager::Key, impl::TextureManager::InternalKey>;

} // namespace ptgn