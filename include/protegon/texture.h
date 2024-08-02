#pragma once

#include <cstdint>

#include "protegon/file.h"
#include "protegon/surface.h"
#include "protegon/vector2.h"
#include "utility/handle.h"

struct SDL_Surface;

namespace ptgn {

class Text;

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

	Texture(const path& image_path, ImageFormat format = ImageFormat::RGBA8888);
	Texture(const Surface& surface);
	Texture(const void* pixel_data, const V2_int& size, ImageFormat format);
	Texture(const std::vector<Color>& pixels, const V2_int& size, ImageFormat format);

	void SetSubData(const void* pixel_data, ImageFormat format);
	void SetSubData(const std::vector<Color>& pixels, ImageFormat format);

	V2_int GetSize() const;

	void Bind() const;
	void Bind(std::uint32_t slot) const;

private:
	friend class impl::RendererData;
	friend class Renderer;
	friend class Text;

	// Does not free surface.
	Texture(const std::shared_ptr<SDL_Surface>& surface);

	static std::int32_t BoundId();

	// static void Unbind();

	void SetDataImpl(const void* pixel_data, const V2_int& size, ImageFormat format);
};

} // namespace ptgn