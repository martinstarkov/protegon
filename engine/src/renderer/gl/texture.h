#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <vector>

#include "core/asset/asset_manager.h"
#include "core/ecs/components/generic.h"
#include "core/ecs/entity.h"
#include "core/util/file.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "renderer/api/flip.h"
#include "serialization/json/enum.h"

struct SDL_Surface;

namespace ptgn {

class Entity;

namespace impl {

class Texture;

} // namespace impl

// @param coordinate Pixel coordinate from [0, size).
// @return Color value of the given pixel.
[[nodiscard]] Color GetPixel(const path& texture_filepath, const V2_int& coordinate);

// @param callback Function to be called for each pixel.
// @return The pixel size of the looped texture.
V2_int ForEachPixel(
	const path& texture_filepath, const std::function<void(const V2_int&, const Color&)>& function
);

struct TextureHandle : public ResourceHandle {
	using ResourceHandle::ResourceHandle;

	// @param entity Optional parameter for if the texture could be attached to the entity via for
	// example a frame buffer or a texture owning entity. If default value, these functions only
	// rely on the texture handle hash for texture retrieval.
	// TODO: In the future get rid of this in favor of the resource managers owning all resources
	// and holding a nameless list of them with index handles.
	[[nodiscard]] const impl::Texture& GetTexture(const Entity& entity = {}) const;
	[[nodiscard]] impl::Texture& GetTexture(const Entity& entity = {});
	[[nodiscard]] V2_int GetSize(const Entity& entity = {}) const;
};

namespace impl {

struct Surface {
	Surface() = default;
	// IMPORTANT: This function will free the surface.
	explicit Surface(SDL_Surface* sdl_surface);

	explicit Surface(const path& filepath);

	// Mirrors the surface vertically.
	void FlipVertically();

	// @param coordinate Pixel coordinate from [0, size).
	// @return Color value of the given pixel.
	[[nodiscard]] Color GetPixel(const V2_int& coordinate) const;

	// @param callback Function to be called for each pixel.
	void ForEachPixel(const std::function<void(const V2_int&, const Color&)>& function) const;

	TextureFormat format{ TextureFormat::Unknown };

	std::size_t bytes_per_pixel{ 0 };
	// The row major one dimensionalized array of pixel values that makes up the surface.
	std::vector<std::uint8_t> data;

	V2_int size;

private:
	// @param index One dimensionalized index into the data array.
	[[nodiscard]] Color GetPixel(std::size_t index) const;
};

[[nodiscard]] static constexpr std::array<V2_float, 4> GetDefaultTextureCoordinates() {
	return {
		V2_float{ 0.0f, 0.0f },
		V2_float{ 1.0f, 0.0f },
		V2_float{ 1.0f, 1.0f },
		V2_float{ 0.0f, 1.0f },
	};
}

[[nodiscard]] std::array<V2_float, 4> GetTextureCoordinates(
	const V2_float& source_position, const V2_float& source_size, const V2_float& texture_size,
	bool offset_texels = false
);

void FlipTextureCoordinates(std::array<V2_float, 4>& texture_coords, Flip flip);

class Texture {
public:
	explicit Texture(const Surface& surface);

	Texture(
		const void* data, const V2_int& size, TextureFormat format = TextureFormat::RGBA8888,
		int mipmap_level = 0, TextureWrapping wrapping_x = TextureWrapping::ClampEdge,
		TextureWrapping wrapping_y = TextureWrapping::ClampEdge,
		TextureScaling minifying   = TextureScaling::Nearest,
		TextureScaling magnifying = TextureScaling::Nearest, bool mipmaps = false
	);

	// Set a sub pixel data of the currently bound texture.
	// @param pixel_data Specifies a pointer to the image data in memory.
	// @param size Specifies the size of the texture subimage (relative to the offset).
	// @param mimap_level Specifies the level-of-detail number. Level 0 is the base image level.
	// Level n is the nth mipmap reduction image.
	// @param offset Specifies a texel offset within the texture array (relative to bottom left
	// corner).
	void SetSubData(
		const void* pixel_data, const V2_int& size, int mipmap_level, const V2_int& offset
	);

	// Set the specified texture slot to active.
	static void SetActiveSlot(std::uint32_t slot);

	// @return Id of the currently active texture slot.
	[[nodiscard]] static std::uint32_t GetActiveSlot();

	void Resize(const V2_int& new_size);

	void SetClampBorderColor(const Color& color) const;

private:
	void SetParameterI(TextureParameter parameter, std::int32_t value) const;

	// @return The uncasted integer value corresponding to the given parameter for the currently
	// bound texture.
	[[nodiscard]] std::int32_t GetParameterI(TextureParameter parameter) const;

	// Set the pixel data of the currently bound texture.
	// @param pixel_data Specifies a pointer to the image data in memory.
	// @param size Specifies the size of the texture.
	// @param format Specifies the format of the pixel data.
	// @param mimap_level Specifies the level-of-detail number. Level 0 is the base image level.
	// Level n is the nth mipmap reduction image.
	void SetData(
		const void* pixel_data, const V2_int& size, TextureFormat format, int mipmap_level
	);

	// Ensure that the texture scaling of the currently bound texture is valid for generating
	// mipmaps.
	[[nodiscard]] static bool ValidMinifyingForMipmaps(TextureScaling minifying);

	// Automatically generate mipmaps for the currently bound texture.
	void GenerateMipmaps() const;

	TextureId id_{ 0 };
	V2_int size_;
	TextureFormat format_{ TextureFormat::Unknown };
};

class TextureManager : public ResourceManager<TextureManager, TextureHandle, Texture> {
public:
	// @return Size of the texture.
	[[nodiscard]] V2_int GetSize(const TextureHandle& key) const;

private:
	[[nodiscard]] const Texture& Get(const TextureHandle& key) const;

	friend ParentManager;
	friend struct ptgn::TextureHandle;

	[[nodiscard]] static Texture LoadFromFile(const path& filepath);
};

} // namespace impl

PTGN_SERIALIZER_REGISTER_ENUM(
	TextureFormat, { { TextureFormat::Unknown, "unknown" },
					 { TextureFormat::HDR_RGB, "hdr_rgb" },
					 { TextureFormat::HDR_RGBA, "hdr_rgba" },
					 { TextureFormat::RGB888, "rgb888" },
					 { TextureFormat::RGBA8888, "rgba8888" },
					 { TextureFormat::BGRA8888, "bgra8888" },
					 { TextureFormat::BGR888, "bgr888" },
					 { TextureFormat::ABGR8888, "abgr8888" },
					 { TextureFormat::ARGB8888, "argb8888" },
					 { TextureFormat::A8, "a8" } }
);

PTGN_SERIALIZER_REGISTER_ENUM(
	TextureWrapping, { { TextureWrapping::ClampEdge, "clamp_edge" },
					   { TextureWrapping::ClampBorder, "clamp_border" },
					   { TextureWrapping::Repeat, "repeat" },
					   { TextureWrapping::MirroredRepeat, "mirrored_repeat" } }
);

PTGN_SERIALIZER_REGISTER_ENUM(
	TextureScaling, { { TextureScaling::Nearest, "nearest" },
					  { TextureScaling::Linear, "linear" },
					  { TextureScaling::NearestMipmapNearest, "nearest_mipmap_nearest" },
					  { TextureScaling::NearestMipmapLinear, "nearest_mipmap_linear" },
					  { TextureScaling::LinearMipmapNearest, "linear_mipmap_nearest" },
					  { TextureScaling::LinearMipmapLinear, "linear_mipmap_linear" } }
);

namespace impl {

PTGN_SERIALIZER_REGISTER_ENUM(
	InternalGLFormat, { { InternalGLFormat::RGBA8, "rgba8" },
						{ InternalGLFormat::R8, "r8" },
						{ InternalGLFormat::RGB8, "rgb8" },
						{ InternalGLFormat::HDR_RGBA, "hdr_rgba" },
						{ InternalGLFormat::HDR_RGB, "hdr_rgb" },
						{ InternalGLFormat::DEPTH24_STENCIL8, "depth24_stencil8" },
						{ InternalGLFormat::STENCIL8, "stencil8" },
						{ InternalGLFormat::DEPTH24, "depth24" } }
);

PTGN_SERIALIZER_REGISTER_ENUM(
	InputGLFormat, { { InputGLFormat::RGBA, "rgba" },
					 { InputGLFormat::SingleChannel, "single_channel" },
					 { InputGLFormat::RGB, "rgb" },
					 { InputGLFormat::BGR, "bgr" },
					 { InputGLFormat::BGRA, "bgra" } }
);

PTGN_SERIALIZER_REGISTER_ENUM(TextureTarget, { { TextureTarget::Texture2D, "texture2d" } });

PTGN_SERIALIZER_REGISTER_ENUM(
	TextureLevelParameter, { { TextureLevelParameter::InternalFormat, "internal_format" } }
);

PTGN_SERIALIZER_REGISTER_ENUM(
	TextureParameter, { { TextureParameter::BorderColor, "border_color" },
						{ TextureParameter::Width, "width" },
						{ TextureParameter::Height, "height" },
						{ TextureParameter::WrapS, "wrap_s" },
						{ TextureParameter::WrapT, "wrap_t" },
						{ TextureParameter::WrapR, "wrap_r" },
						{ TextureParameter::MagnifyingScaling, "magnifying_scaling" },
						{ TextureParameter::MinifyingScaling, "minifying_scaling" } }
);

} // namespace impl

} // namespace ptgn