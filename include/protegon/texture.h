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

enum class TextureSmoothing {
	Linear	= 0x2601, // GL_LINEAR
	Nearest = 0x2600  // GL_NEAREST
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
	constexpr const static TextureSmoothing default_smoothing{ TextureSmoothing::Nearest };

public:
	Texture(
		const path& image_path, ImageFormat format = ImageFormat::RGBA8888,
		TextureSmoothing smoothing = default_smoothing
	);
	Texture(const Surface& surface, TextureSmoothing smoothing = default_smoothing);
	Texture(
		const void* pixel_data, const V2_int& size, ImageFormat format,
		TextureSmoothing smoothing = default_smoothing
	);
	Texture(
		const std::vector<Color>& pixels, const V2_int& size,
		TextureSmoothing smoothing = default_smoothing
	);

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