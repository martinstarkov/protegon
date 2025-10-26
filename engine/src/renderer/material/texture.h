#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <vector>

#include "core/ecs/components/generic.h"
#include "core/ecs/entity.h"
#include "core/asset/asset_manager.h"
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

// Format of pixels for a texture or surface.
// e.g. RGBA8888 means 8 bits per color channel (32 bits total).
enum class TextureFormat : std::uint32_t {
	Unknown	 = 0, // SDL_PIXELFORMAT_UNKNOWN
	HDR_RGB	 = 999999998,
	HDR_RGBA = 999999999,
	RGB888	 = 370546692, // SDL_PIXELFORMAT_RGB888
	RGBA8888 = 373694468, // SDL_PIXELFORMAT_RGBA8888
	BGRA8888 = 377888772, // SDL_PIXELFORMAT_BGRA8888
	BGR888	 = 374740996, // SDL_PIXELFORMAT_BGR888
	ABGR8888 = 376840196, // SDL_PIXELFORMAT_ABGR8888
	ARGB8888 = 372645892, // SDL_PIXELFORMAT_ARGB8888
	A8		 = 318769153, // SDL_PIXELFORMAT_INDEX8 // Only alpha values.
	Depth24,
	Depth24_Stencil8
};

enum class TextureWrapping {
	ClampEdge	   = 0x812F, // GL_CLAMP_TO_EDGE
	ClampBorder	   = 0x812D, // GL_CLAMP_TO_BORDER
	Repeat		   = 0x2901, // GL_REPEAT
	MirroredRepeat = 0x8370	 // GL_MIRRORED_REPEAT
};

enum class TextureScaling {
	Nearest				 = 0x2600, // GL_NEAREST
	Linear				 = 0x2601, // GL_LINEAR
	NearestMipmapNearest = 0x2700, // GL_NEAREST_MIPMAP_NEAREST
	NearestMipmapLinear	 = 0x2702, // GL_NEAREST_MIPMAP_LINEAR
	LinearMipmapNearest	 = 0x2701, // GL_LINEAR_MIPMAP_NEAREST
	LinearMipmapLinear	 = 0x2703, // GL_LINEAR_MIPMAP_LINEAR
};

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

enum class InternalGLFormat : std::int32_t {
	R8				 = 0x8229, // GL_R8
	RGB8			 = 0x8051, // GL_RGB8
	RGBA8			 = 0x8058, // GL_RGBA8
	HDR_RGBA		 = 0x881A, // GL_RGBA16F
	HDR_RGB			 = 0x881B, // GL_RGB16F
	DEPTH24_STENCIL8 = 0x88F0, // GL_DEPTH24_STENCIL8 AND GL_DEPTH24_STENCIL8_OES
	STENCIL8		 = 0x8D48, // GL_STENCIL_INDEX8
	DEPTH24			 = 0x81A6  // GL_DEPTH_COMPONENT24
};

enum class InputGLFormat {
	SingleChannel = 0x1903, // GL_RED
	RGB			  = 0x1907, // GL_RGB
	RGBA		  = 0x1908, // GL_RGBA
	BGR			  = 0x80E0, // GL_BGR
	BGRA		  = 0x80E1, // GL_BGRA
	DepthStencil  = 0x821A, //  GL_DEPTH_STENCIL
	Depth		  = 0x8D00, // GL_DEPTH_COMPONENT
							// Stencil		 = 0x8D20,	//  GL_STENCIL_INDEX // Not allowed for textures.
};

enum class TextureTarget {
	Texture2D = 0x0DE1 // GL_TEXTURE_2D
};

enum class TextureLevelParameter {
	InternalFormat = 0x1003 // GL_TEXTURE_INTERNAL_FORMAT
};

enum class TextureParameter {
	BorderColor		  = 0x1004, // GL_TEXTURE_BORDER_COLOR
	Width			  = 0x1000, // GL_TEXTURE_WIDTH
	Height			  = 0x1001, // GL_TEXTURE_HEIGHT
	WrapS			  = 0x2802, // GL_TEXTURE_WRAP_S (x)
	WrapT			  = 0x2803, // GL_TEXTURE_WRAP_T (y)
	WrapR			  = 0x8072, // GL_TEXTURE_WRAP_R (z)
	MagnifyingScaling = 0x2800, // GL_TEXTURE_MAG_FILTER
	MinifyingScaling  = 0x2801, // GL_TEXTURE_MIN_FILTER
};

struct GLFormats {
	// Storage format of the OpenGL texture.
	InternalGLFormat internal_format{ InternalGLFormat::RGBA8 };
	// Input format of the pixel data to the texture.
	InputGLFormat input_format{ InputGLFormat::RGBA };
	// Number of color components that make up the texture pixel (e.g. RGB has 3).
	int color_components{ 4 };
};

[[nodiscard]] TextureFormat GetFormatFromOpenGL(InternalGLFormat opengl_internal_format);

[[nodiscard]] TextureFormat GetFormatFromSDL(std::uint32_t sdl_format);

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

[[nodiscard]] GLFormats GetGLFormats(TextureFormat format);

using TextureId = std::uint32_t;

class Texture {
public:
	Texture() = default;

	explicit Texture(const Surface& surface);

	Texture(
		const void* data, const V2_int& size, TextureFormat format = TextureFormat::RGBA8888,
		int mipmap_level = 0, TextureWrapping wrapping_x = TextureWrapping::ClampEdge,
		TextureWrapping wrapping_y = TextureWrapping::ClampEdge,
		TextureScaling minifying   = TextureScaling::Nearest,
		TextureScaling magnifying = TextureScaling::Nearest, bool mipmaps = false
	);
	Texture(const Texture&)			   = delete;
	Texture& operator=(const Texture&) = delete;
	Texture(Texture&& other) noexcept;
	Texture& operator=(Texture&& other) noexcept;
	~Texture();

	friend bool operator==(const Texture& a, const Texture& b) {
		return a.id_ == b.id_;
	}

	// @return Size of the texture.
	[[nodiscard]] V2_int GetSize() const;

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

	// Set the specified texture slot to active and bind the texture id to that slot.
	static void Bind(TextureId id, std::uint32_t slot);

	static void BindId(TextureId id);

	// Set the specified texture slot to active and bind the texture to that slot.
	void Bind(std::uint32_t slot) const;

	// Bind the texture to the currently active texture slot.
	void Bind() const;

	// Set the specified texture slot to active and bind the texture of that slot to 0.
	static void Unbind(std::uint32_t slot);

	// @return Id of the texture bound to the currently active texture slot.
	[[nodiscard]] static TextureId GetBoundId();

	// @return True if the texture is currently bound, false otherwise.
	[[nodiscard]] bool IsBound() const;

	// Set the specified texture slot to active.
	static void SetActiveSlot(std::uint32_t slot);

	// @return Id of the currently active texture slot.
	[[nodiscard]] static std::uint32_t GetActiveSlot();

	// @return Id of the texture object.
	[[nodiscard]] TextureId GetId() const;

	// @return True if id != 0.
	[[nodiscard]] bool IsValid() const;

	[[nodiscard]] TextureFormat GetFormat() const;

	void Resize(const V2_int& new_size);

	void SetClampBorderColor(const Color& color) const;

private:
	void GenerateTexture();
	void DeleteTexture() noexcept;

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