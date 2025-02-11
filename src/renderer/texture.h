#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/flip.h"
#include "utility/file.h"

struct SDL_Surface;

namespace ptgn {

// Format of pixels for a texture or surface.
// e.g. RGBA8888 means 8 bits per color channel (32 bits total).
enum class TextureFormat : std::uint32_t {
	Unknown	 = 0,		  // SDL_PIXELFORMAT_UNKNOWN
	RGB888	 = 370546692, // SDL_PIXELFORMAT_RGB888
	RGBA8888 = 373694468, // SDL_PIXELFORMAT_RGBA8888
	BGRA8888 = 377888772, // SDL_PIXELFORMAT_BGRA8888
	BGR888	 = 374740996, // SDL_PIXELFORMAT_BGR888
	ABGR8888 = 376840196, // SDL_PIXELFORMAT_ABGR8888
	ARGB8888 = 372645892, // SDL_PIXELFORMAT_ARGB8888
	A8		 = 318769153, // SDL_PIXELFORMAT_INDEX8 // Only alpha values.
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

namespace impl {

class RenderData;

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

enum class InternalGLFormat {
	R8	  = 0x8229, // GL_R8
	RGB8  = 0x8051, // GL_RGB8
	RGBA8 = 0x8058, // GL_RGBA8
};

enum class InputGLFormat {
	SingleChannel = 0x1903, // GL_RED
	RGB			  = 0x1907, // GL_RGB
	RGBA		  = 0x1908, // GL_RGBA
	BGR			  = 0x80E0, // GL_BGR
	BGRA		  = 0x80E1, // GL_BGRA
};

enum class InternalGLDepthFormat {
	DEPTH24_STENCIL8 = 0x88F0 // GL_DEPTH24_STENCIL8 AND GL_DEPTH24_STENCIL8_OES
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

[[nodiscard]] GLFormats GetGLFormats(TextureFormat format);

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

	bool operator==(const Texture& other) const;
	bool operator!=(const Texture& other) const;

	// @return Size of the texture.
	[[nodiscard]] V2_int GetSize() const;

	// Set a sub pixel data of the currently bound texture.
	// @param pixel_data Specifies a pointer to the image data in memory.
	// @param size Specifies the size of the texture subimage (relative to the offset).
	// @param format Specifies the format of the pixel data.
	// @param mimap_level Specifies the level-of-detail number. Level 0 is the base image level.
	// Level n is the nth mipmap reduction image.
	// @param offset Specifies a texel offset within the texture array (relative to bottom left
	// corner).
	void SetSubData(
		const void* pixel_data, const V2_int& size, TextureFormat format, int mipmap_level,
		const V2_int& offset
	);

	// Set the specified texture slot to active and bind the texture id to that slot.
	static void Bind(std::uint32_t id, std::uint32_t slot);

	static void BindId(std::uint32_t id);

	// Set the specified texture slot to active and bind the texture to that slot.
	void Bind(std::uint32_t slot) const;

	// Bind the texture to the currently active texture slot.
	void Bind() const;

	// Set the specified texture slot to active and bind the texture of that slot to 0.
	static void Unbind(std::uint32_t slot);

	// @return Id of the texture bound to the currently active texture slot.
	[[nodiscard]] static std::uint32_t GetBoundId();

	// @return True if the texture is currently bound, false otherwise.
	[[nodiscard]] bool IsBound() const;

	// Set the specified texture slot to active.
	static void SetActiveSlot(std::uint32_t slot);

	// @return Id of the currently active texture slot.
	[[nodiscard]] static std::uint32_t GetActiveSlot();

	// @return Id of the texture object.
	[[nodiscard]] std::uint32_t GetId() const;

	// @return True if id != 0.
	[[nodiscard]] bool IsValid() const;

	[[nodiscard]] TextureFormat GetFormat() const;

private:
	void GenerateTexture();
	void DeleteTexture() noexcept;

	void SetParameterI(TextureParameter parameter, std::int32_t value) const;

	// @return The uncasted integer value corresponding to the given parameter for the currently
	// bound texture.
	[[nodiscard]] std::int32_t GetParameterI(TextureParameter parameter) const;

	[[nodiscard]] std::int32_t GetLevelParameterI(
		TextureLevelParameter parameter, std::int32_t level
	) const;

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

	std::uint32_t id_{ 0 };
	V2_int size_;
};

class TextureManager {
public:
	// If key exists in the texture manager, does nothing.
	void Load(std::string_view key, const path& filepath);

	void Unload(std::string_view key);

	// @return Size of the texture.
	[[nodiscard]] V2_int GetSize(std::string_view key) const;

	// @return True if the texture key is loaded.
	[[nodiscard]] bool Has(std::string_view key) const;

private:
	friend class RenderData;

	[[nodiscard]] const Texture& Get(std::size_t key) const;

	[[nodiscard]] bool Has(std::size_t key) const;

	[[nodiscard]] static Texture LoadFromFile(const path& filepath);

	// TODO: Fix.
	// Draws the texture to the currently bound frame buffer.
	/*void DrawToBoundFrameBuffer(
		std::string_view key, const Rect& destination, const TextureInfo& texture_info,
		const Shader& shader
	) const;*/

	std::unordered_map<std::size_t, Texture> textures_;
};

} // namespace impl

} // namespace ptgn