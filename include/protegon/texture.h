#pragma once

#include <cstdint>

#include "protegon/file.h"
#include "protegon/surface.h"
#include "protegon/vector2.h"
#include "utility/handle.h"

namespace ptgn {

namespace impl {

struct GLFormats {
	// first
	std::int32_t internal_{ 0 };
	// second
	std::uint32_t format_{ 0 };
};

static GLFormats GetGLFormats(ImageFormat format);

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

	Texture(
		const path& image_path, bool flip_vertically = true,
		ImageFormat format = ImageFormat::RGBA8888
	);
	Texture(const Surface& surface);
	Texture(void* pixel_data, const V2_int& size, ImageFormat format);

	void SetData(void* pixel_data, const V2_int& size, ImageFormat format);
	void SetSubData(void* pixel_data, ImageFormat format, const V2_int& offset = {});

	void Bind() const;
	void Bind(std::uint32_t slot) const;
	void Unbind() const;

	V2_int GetSize() const;

private:
	void SetDataImpl(void* pixel_data, const V2_int& size, ImageFormat format);
};

} // namespace ptgn