#pragma once

#include <cstdint>
#include <variant>
#include <vector>

#include "core/manager.h"
#include "protegon/color.h"
#include "protegon/file.h"
#include "protegon/surface.h"
#include "protegon/vector2.h"
#include "utility/handle.h"

namespace ptgn {

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

class Renderer;

namespace impl {

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
	// first
	InternalGLFormat internal_{ InternalGLFormat::RGBA8 };
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
	Texture()			= default;
	~Texture() override = default;

private:
	constexpr const static TextureFilter default_minifying_filter{ TextureFilter::Nearest };
	constexpr const static TextureFilter default_magnifying_filter{ TextureFilter::Nearest };
	constexpr const static TextureWrapping default_wrapping{ TextureWrapping::ClampEdge };

public:
	Texture(const path& image_path, ImageFormat format = ImageFormat::RGBA8888);
	explicit Texture(const Surface& surface);
	Texture(const void* pixel_data, const V2_int& size, ImageFormat format);
	Texture(const std::vector<Color>& pixels, const V2_int& size);

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

	void Bind() const;
	void Bind(std::uint32_t slot) const;
	void SetActiveSlot(std::uint32_t slot) const;

private:
	friend class impl::TextureBatchData;
	friend class impl::RendererData;
	friend class Renderer;

	[[nodiscard]] static std::int32_t GetBoundId();
	[[nodiscard]] static std::int32_t GetActiveSlot();

	void SetDataImpl(const void* pixel_data, const V2_int& size, ImageFormat format);
};

namespace impl {

class TextureManager : public Manager<Texture> {
public:
	using Manager::Manager;
};

} // namespace impl

using TextureOrKey =
	std::variant<Texture, impl::TextureManager::Key, impl::TextureManager::InternalKey>;

} // namespace ptgn