#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <variant>
#include <vector>

#include "core/manager.h"
#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/flip.h"
#include "renderer/surface.h"
#include "utility/file.h"
#include "utility/handle.h"

namespace ptgn {

class Text;
class RenderTarget;
struct LayerInfo;

// Information relating to the source pixels, flip, tinting and rotation center of the texture.
struct TextureInfo {
	TextureInfo() = default;

	/*
	@param flip Mirror the texture along an axis.
	*/
	explicit TextureInfo(Flip flip) : flip{ flip } {}

	/*
	@param tint Color to tint the texture. Allows to change the transparency of a texture.
	(color::White corresponds to no tint effect).
	*/
	explicit TextureInfo(const Color& tint) : tint{ tint } {}

	/*
	@param source_position Top left pixel to start drawing texture from within the texture.
	@param source_size Number of pixels of the texture to draw ({} which corresponds to the
	remaining texture size to the bottom right of source_position). source.origin Relative  to
	destination_position the direction from which the texture is.
	@param flip Mirror the texture along an axis.
	@param tint Color to tint the texture. Allows to change the transparency of a texture.
	(color::White corresponds to no tint effect).
	@param rotation_center Fraction of the source_size around which the texture is rotated ({ 0.5f,
	0.5f } corresponds to the center of the texture).
	*/
	TextureInfo(
		const V2_float& source_position, const V2_float& source_size, Flip flip = Flip::None,
		const Color& tint = color::White, const V2_float& rotation_center = { 0.5f, 0.5f }
	) :
		source_position{ source_position },
		source_size{ source_size },
		flip{ flip },
		tint{ tint },
		rotation_center{ rotation_center } {}

	V2_float source_position;
	V2_float source_size;
	Flip flip{ Flip::None };
	Color tint{ color::White };
	V2_float rotation_center{ 0.5f, 0.5f };

private:
	friend class RenderTarget;

	[[nodiscard]] std::array<V2_float, 4> GetTextureCoordinates(
		const V2_float& texture_size, bool offset_texels = false
	) const;

	static void FlipTextureCoordinates(std::array<V2_float, 4>& texture_coords, Flip flip);
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

struct Batch;
class RenderData;
struct FrameBufferInstance;
struct RenderTargetInstance;

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
	MagScaling	= 0x2800, // GL_TEXTURE_MAG_FILTER
	MinScaling	= 0x2801, // GL_TEXTURE_MIN_FILTER
};

struct GLFormats {
	InternalGLFormat internal_{ InternalGLFormat::RGBA8 };
	std::uint32_t format_{ 0 };
	int components_{ 0 };
};

[[nodiscard]] GLFormats GetGLFormats(TextureFormat format);

struct TextureInstance {
	TextureInstance(
		const void* pixel_data, const V2_int& size, TextureFormat format,
		TextureWrapping wrapping_x, TextureWrapping wrapping_y, TextureScaling minifying,
		TextureScaling magnifying, bool mipmaps, bool resize_with_window
	);
	~TextureInstance();

	// Minifying TextureScaling must contain the word Mipmap, otherwise mipmaps are ignored.
	void CreateTexture(
		const void* pixel_data, const V2_int& size, TextureFormat format,
		TextureWrapping wrapping_x, TextureWrapping wrapping_y, TextureScaling minifying,
		TextureScaling magnifying, bool mipmaps
	);

	// @param mimap_level Specifies the level-of-detail number. Level 0 is the base image level.
	// Level n is the nth mipmap reduction image.
	void SetData(
		const void* pixel_data, const V2_int& size, TextureFormat format, int mipmap_level
	);

	void SetWrappingX(TextureWrapping x);
	void SetWrappingY(TextureWrapping y);
	void SetWrappingZ(TextureWrapping z);

	void SetScalingMinifying(TextureScaling minifying);
	void SetScalingMagnifying(TextureScaling magnifying);

	void GenerateMipmaps();

	void Bind() const;

	[[nodiscard]] bool IsBound() const;

	[[nodiscard]] static bool ValidMinifyingForMipmaps(TextureScaling minifying);

	std::uint32_t id_{ 0 };
	V2_int size_;
	TextureFormat format_{ TextureFormat::RGBA8888 };

	// OpenGL Equivalent Coordinate: S
	TextureWrapping wrapping_x_{ TextureWrapping::ClampEdge };

	// OpenGL Equivalent Coordinate: T
	TextureWrapping wrapping_y_{ TextureWrapping::ClampEdge };

	// OpenGL Equivalent Coordinate: R
	TextureWrapping wrapping_z_{ TextureWrapping::ClampEdge };

	TextureScaling minifying_{ TextureScaling::Nearest };
	TextureScaling magnifying_{ TextureScaling::Nearest };
	bool mipmaps_{ false };
};

} // namespace impl

class Texture : public Handle<impl::TextureInstance> {
public:
	// Default values for texture format, scaling, and wrapping.

	constexpr const static TextureFormat default_format{ TextureFormat::RGBA8888 };
	constexpr const static TextureScaling default_minifying_scaling{ TextureScaling::Nearest };
	constexpr const static TextureScaling default_magnifying_scaling{ TextureScaling::Nearest };
	constexpr const static TextureWrapping default_wrapping{ TextureWrapping::ClampEdge };

	Texture() = default;

	~Texture() override = default;

	// @param image_path Path to the texture relative to the working directory.
	Texture(const path& image_path);

	// @param pixels One dimensionalized array of pixels containing the texture data.
	// @param size Size of the texture. Area must match length of pixels array.
	Texture(const std::vector<Color>& pixels, const V2_int& size);

	// WARNING: This is a very slow operation, and should not be used frequently. If reading pixels
	// from a regular texture, prefer to create a Surface{ path } and read pixels from that instead.
	// This function is primarily for debugging render targets.
	// @param coordinate Pixel coordinate from [0, size).
	// @return Color value of the given pixel.
	// Note: Only RGB/RGBA format textures supported.
	[[nodiscard]] Color GetPixel(const V2_int& coordinate) const;

	// WARNING: This is a very slow operation, and should not be used frequently. If reading pixels
	// from a regular texture, prefer to create a Surface{ path } and read pixels from that instead.
	// This function is primarily for debugging render targets.
	// @param callback Function to be called for each pixel.
	// Note: Only RGB/RGBA format textures supported.
	void ForEachPixel(const std::function<void(V2_int, Color)>& callback) const;

	// @param destination Destination to draw the texture to. If destination == {}, fullscreen
	// texture will be drawn, else if destination.size == {}, unscaled texture size is used.
	// @param texture_info Information relating to the source pixels, flip, tinting and rotation
	// center of the texture.
	// Uses default render target.
	void Draw(const Rect& destination = {}, const TextureInfo& texture_info = {}) const;

	// @param destination Destination to draw the texture to. If destination == {}, fullscreen
	// texture will be drawn, else if destination.size == {}, unscaled texture size is used.
	// @param texture_info Information relating to the source pixels, flip, tinting and rotation
	// center of the texture.
	// @param layer_info Information relating to the z index and render target of the texture.
	void Draw(const Rect& destination, const TextureInfo& texture_info, const LayerInfo& layer_info)
		const;

	// @param wrapping Texture wrapping in the x direction for when texture X coordinates are
	// outside [0, 1]. OpenGL Equivalent Coordinate: S.
	void SetWrappingX(TextureWrapping x);

	// @param wrapping Texture wrapping in the y direction for when texture Y coordinates are
	// outside [0, 1]. OpenGL Equivalent Coordinate: T.
	void SetWrappingY(TextureWrapping y);

	// @param wrapping Texture wrapping in the z direction for when texture Z coordinates are
	// outside [0, 1]. OpenGL Equivalent Coordinate: R.
	void SetWrappingZ(TextureWrapping z);

	// @param minifying Scaling when the texture is displayed smaller than its original size.
	void SetScalingMinifying(TextureScaling minifying);

	// @param magnifying Scaling when the texture is displayed larger than its original size.
	void SetScalingMagnifying(TextureScaling magnifying);

	// @param color Texture color when using TextureWrapping::ClampBorder and texture coordinates
	// are outside [0, 1].
	void SetClampBorderColor(const Color& color) const;

	// Automatically generate mipmaps for the texture.
	void GenerateMipmaps();

	// Set a subimage of the texture.
	// @param pixels One dimensionalized array of pixels containing the texture data.
	// @param offset Specifies a texel offset within the texture array (relative to bottom left
	// corner).
	// @param size Specifies the size of the texture subimage (relative to the offset).
	// @param mimap_level Specifies the level-of-detail number. Level 0 is the base image level.
	// Level n is the nth mipmap reduction image.
	void SetSubData(
		const std::vector<Color>& pixels, const V2_int& offset, const V2_int& size,
		int mipmap_level = 0
	);

	// @return Size of the texture.
	[[nodiscard]] V2_int GetSize() const;

	// @return Pixel format of the texture.
	[[nodiscard]] TextureFormat GetFormat() const;

private:
	friend class Text;
	friend struct impl::FrameBufferInstance;
	friend struct impl::TextureInstance;
	friend struct impl::Batch;
	friend class impl::RenderData;
	friend struct impl::RenderTargetInstance;

	struct WindowTexture {};

	// Bind the texture to the currently active texture slot. This function does not change the
	// active texture slot.
	void Bind() const;

	// Set the specified texture slot to active and bind the texture to that slot.
	void Bind(std::uint32_t slot) const;

	// Set the specified texture slot to active and bind the texture of that slot to 0.
	static void Unbind(std::uint32_t slot = 0);

	// Set the specified texture slot to active.
	static void SetActiveSlot(std::uint32_t slot);

	// Creates a window sized texture. Used for drawing to render targets.
	explicit Texture(const WindowTexture& window_texture);

	// @param size Manually set the size of an empty texture.
	explicit Texture(const V2_float& size);

	// @param surface Construct texture from a surface. Used internally when creating text.
	explicit Texture(const Surface& surface);

	// @param pixel_data Pointer to pixel data array of the texture.
	// @param size Size of the texture.
	// @param format Specifies the format of the pixel data.
	// @param wrapping_x Wrapping along X axis for when texture coordinates are outside [0, 1].
	// @param wrapping_y Wrapping along Y axis for when texture coordinates are outside [0, 1].
	// @param minifying Scaling when the texture is displayed smaller than its original size.
	// @param magnifying Scaling when the texture is displayed larger than its original size.
	// @param mipmaps Whether or not to generate mipmaps for the texture. Note: Minifying
	// TextureScaling must contain the word Mipmap, otherwise mipmaps are ignored.
	// @param resize_with_window If true, sets the texture to resize continously to the
	// window size. Used for drawing to render targets.
	Texture(
		const void* pixel_data, const V2_int& size, TextureFormat format = default_format,
		TextureWrapping wrapping_x = default_wrapping,
		TextureWrapping wrapping_y = default_wrapping,
		TextureScaling minifying   = default_minifying_scaling,
		TextureScaling magnifying = default_minifying_scaling, bool mipmaps = false,
		bool resize_with_window = false
	);

	// Set a subimage of the texture.
	// @param pixel_data Specifies a pointer to the image data in memory.
	// @param format Specifies the format of the pixel data.
	// @param offset Specifies a texel offset within the texture array (relative to bottom left
	// corner).
	// @param size Specifies the size of the texture subimage (relative to the offset).
	// @param mimap_level Specifies the level-of-detail number. Level 0 is the base image level.
	// Level n is the nth mipmap reduction image.
	void SetSubData(
		const void* pixel_data, TextureFormat format, const V2_int& offset, const V2_int& size,
		int mipmap_level = 0
	);

	// @return The id of the currently active texture slot.
	[[nodiscard]] static std::int32_t GetActiveSlot();

	// @return The id of the texture bound to the currently active texture slot.
	[[nodiscard]] static std::int32_t GetBoundId();

	// @return True if the texture is bound to the currently active texture slot.
	[[nodiscard]] bool IsBound() const;
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