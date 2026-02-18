#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "core/util/file.h"
#include "core/util/id_map.h"
#include "renderer/resources/framebuffer.h"
#include "renderer/resources/renderbuffer.h"
#include "renderer/resources/texture.h"

namespace ptgn::impl::gl {

class GLContext;

enum class AttachmentObject : std::uint32_t {
	None		 = 0,
	Texture2D	 = 0x0DE1, // GL_TEXTURE_2D
	Renderbuffer = 0x8D41  // GL_RENDERBUFFER
};

struct AttachmentSpec {
	std::uint32_t id{ 0 };
	AttachmentObject object{ AttachmentObject::None };
};

struct FramebufferCache {
	std::array<AttachmentSpec, 8> color;
	AttachmentSpec depth;
	AttachmentSpec stencil;
	AttachmentSpec depth_stencil;
};

enum class Attachment : std::uint32_t {
	// Color attachments
	Color0 = 0x8CE0, // GL_COLOR_ATTACHMENT0
	Color1 = 0x8CE1, // GL_COLOR_ATTACHMENT1
	Color2 = 0x8CE2, // GL_COLOR_ATTACHMENT2
	Color3 = 0x8CE3, // GL_COLOR_ATTACHMENT3
	Color4 = 0x8CE4, // GL_COLOR_ATTACHMENT4
	Color5 = 0x8CE5, // GL_COLOR_ATTACHMENT5
	Color6 = 0x8CE6, // GL_COLOR_ATTACHMENT6
	Color7 = 0x8CE7, // GL_COLOR_ATTACHMENT7
	Color8 = 0x8CE8, // GL_COLOR_ATTACHMENT8

	// Depth / Stencil attachments
	Depth		 = 0x8D00, // GL_DEPTH_ATTACHMENT
	Stencil		 = 0x8D20, // GL_STENCIL_ATTACHMENT
	DepthStencil = 0x821A  // GL_DEPTH_STENCIL_ATTACHMENT
};

enum class ClearBufferBit : std::uint32_t {
	None	= 0,
	Color	= 0x00004000, // GL_COLOR_BUFFER_BIT
	Depth	= 0x00000100, // GL_DEPTH_BUFFER_BIT
	Stencil = 0x00000400  // GL_STENCIL_BUFFER_BIT
};

enum class ClearBufferType : std::uint32_t {
	Color	= 0x1800, // GL_COLOR
	Depth	= 0x1801, // GL_DEPTH
	Stencil = 0x1802  // GL_STENCIL
};

constexpr ClearBufferBit operator|(ClearBufferBit a, ClearBufferBit b) {
	return static_cast<ClearBufferBit>(std::to_underlying(a) | std::to_underlying(b));
}

class Framebuffers {
public:
	Framebuffer CreateFramebuffer(
		std::optional<Texture> texture = {}, Attachment texture_attachment = Attachment::Color0,
		std::optional<Renderbuffer> renderbuffer = {},
		Attachment renderbuffer_attachment = Attachment::DepthStencil, bool restore_bind = true
	);

	void DestroyFramebuffer(Framebuffer id);

	void AttachTexture(Framebuffer framebuffer, Texture texture, Attachment attachment);

	void AttachRenderbuffer(
		Framebuffer framebuffer, Renderbuffer renderbuffer, Attachment attachment
	);

	/// Clear buffer bits to preset values
	void Clear(
		ClearBufferBit buffers = ClearBufferBit::Color | ClearBufferBit::Stencil |
								 ClearBufferBit::Depth
	) const;

	/// Clear individual buffers of a framebuffer.
	/// @param buffer Specify the type of buffer to clear.
	/// @param drawbuffer Specify a particular index of draw buffer to clear. Must be 0 for depth
	/// and stencil buffers and within max color attachments for color buffers.
	void ClearToColor(
		Framebuffer framebuffer, Color color, ClearBufferType buffer = ClearBufferType::Color,
		int drawbuffer = 0
	) const;

	// Color
	// Depth -> float
	// Stencil -> uint8_t
	// Depth+Stencil -> {float depth, uint8_t stencil}
	using PixelValue = std::variant<Color, float, std::uint8_t, std::pair<float, std::uint8_t>>;

	// WARNING: This function is slow and should be primarily used for debugging framebuffers.
	// @param coordinate Pixel coordinate from [0, size).
	PixelValue ReadPixel(
		Framebuffer framebuffer, V2_int coordinate, Attachment attachment = Attachment::Color0
	);

	enum class AttachmentType {
		Color,
		Depth,
		Stencil,
		DepthStencil
	};

	struct PixelBuffer {
		V2_int size{};
		AttachmentType type{};
		std::vector<std::uint8_t> data;
	};

	// WARNING: This function is slow and should be primarily used for debugging framebuffers.
	PixelBuffer ReadPixels(Framebuffer framebuffer, Attachment attachment = Attachment::Color0);

	// WARNING: This function is slow and should be primarily used for debugging framebuffers.
	template <typename F>
	void ForEachPixel(
		const PixelBuffer& buffer, F&& func /* (V2_int, PixelValue) */
	) const {
		const auto& data		  = buffer.data;
		const V2_int size		  = buffer.size;
		const AttachmentType type = buffer.type;

		for (int y = 0; y < size.y; ++y) {
			int flipped = size.y - 1 - y;

			for (int x = 0; x < size.x; ++x) {
				const int idx = flipped * size.x + x;
				PixelValue px;

				switch (type) {
					case AttachmentType::Color: {
						const std::uint8_t* p = &data[idx * 4];
						px					  = Color{ p[0], p[1], p[2], p[3] };
						break;
					}

					case AttachmentType::Depth: {
						const float* p = reinterpret_cast<const float*>(data.data());
						px			   = p[idx];
						break;
					}

					case AttachmentType::Stencil: {
						px = data[idx];
						break;
					}

					case AttachmentType::DepthStencil: {
						const auto* p = reinterpret_cast<const std::uint32_t*>(data.data());
						const std::uint32_t packed = p[idx];
						float depth				   = float(packed & 0xFFFFFF) / float(0xFFFFFF);
						std::uint8_t stencil	   = (packed >> 24) & 0xFF;
						px						   = std::make_pair(depth, stencil);
						break;
					}
				}

				func(V2_int{ x, y }, px);
			}
		}
	}

	template <typename F>
	void ForEachPixel(
		Framebuffer framebuffer, F&& func, Attachment attachment = Attachment::Color0
	) {
		PixelBuffer buffer = ReadPixels(framebuffer, attachment);
		ForEachPixel(buffer, std::forward<F>(func));
	}

	void SavePNG(
		const path& path, Framebuffer framebuffer, Attachment attachment = Attachment::Color0
	);

	AttachmentSpec& GetFramebufferAttachment(Framebuffer framebuffer, Attachment attachment);

	const AttachmentSpec& GetFramebufferAttachment(Framebuffer framebuffer, Attachment attachment)
		const;

	void ResizeFramebuffer(Framebuffer framebuffer, V2_int new_size);

private:
	friend class GLContext;

	explicit Framebuffers(GLContext& gl);
	~Framebuffers() noexcept						 = default;
	Framebuffers(const Framebuffers&)				 = delete;
	Framebuffers(Framebuffers&&) noexcept			 = delete;
	Framebuffers& operator=(const Framebuffers&)	 = delete;
	Framebuffers& operator=(Framebuffers&&) noexcept = delete;

	void Init(std::uint32_t max_color_attachments);

	[[nodiscard]] bool FramebufferIsComplete(Framebuffer framebuffer) const;

	[[nodiscard]] const char* GetFramebufferStatus() const;

	AttachmentType GetAttachmentType(Attachment attachment) const;

	void UpdateFramebufferCache(
		Framebuffer framebuffer, std::uint32_t object_id, Attachment attachment,
		AttachmentObject object_type
	);

	[[nodiscard]] Framebuffer CreateFramebufferImpl();

	// TODO: Make sure to update the cache when the parameters change. I.e. when resizing a
	// texture.
	// TODO: Store resource cached values here by GLuint key and erase them in the resource
	// deleter. e.g. std::unordered_map<GLuint, location_cache> location_caches_;
	// UnsafeDelete(); location_caches_.erase(id);

	GLContext& gl_;

	// equivalent to GL_MAX_COLOR_ATTACHMENTS, or the number of color attachments a framebuffer can
	// have. This is set by the constructor and should not be modified afterward.
	std::uint32_t max_color_attachments_{ 0 };

	IdMap<FramebufferCache> cache_;
};

} // namespace ptgn::impl::gl