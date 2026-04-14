#pragma once

#include <array>
#include <concepts>
#include <cstdint>
#include <optional>
#include <ostream>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "core/util/file.h"
#include "core/util/id_map.h"
#include "renderer/resources/id.h"
#include "serialization/serialize.h"

namespace ptgn::impl::gl {

class GLContext;

inline constexpr std::uint32_t kFrameBufferTarget{ 0x8D40 }; // GL_FRAMEBUFFER

enum class AttachmentObject : std::uint32_t {
	None		 = 0,
	Texture2D	 = 0x0DE1, // GL_TEXTURE_2D
	Renderbuffer = 0x8D41  // GL_RENDERBUFFER
};
PTGN_REFLECT_ENUM(AttachmentObject);

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
PTGN_REFLECT_ENUM(Attachment);

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

[[nodiscard]] Attachment ColorAttachment(std::size_t i);

enum class ClearBufferBit : std::uint32_t {
	None	= 0,
	Color	= 0x00004000, // GL_COLOR_BUFFER_BIT
	Depth	= 0x00000100, // GL_DEPTH_BUFFER_BIT
	Stencil = 0x00000400  // GL_STENCIL_BUFFER_BIT
};
PTGN_REFLECT_ENUM_NOSTREAM(ClearBufferBit);
std::ostream& operator<<(std::ostream& os, ClearBufferBit bits);

constexpr ClearBufferBit operator|(ClearBufferBit a, ClearBufferBit b) {
	return static_cast<ClearBufferBit>(std::to_underlying(a) | std::to_underlying(b));
}

constexpr ClearBufferBit operator&(ClearBufferBit a, ClearBufferBit b) {
	return static_cast<ClearBufferBit>(std::to_underlying(a) & std::to_underlying(b));
}

constexpr ClearBufferBit& operator|=(ClearBufferBit& a, ClearBufferBit b) {
	return a = a | b;
}

enum class ClearBufferType : std::uint32_t {
	Color	= 0x1800, // GL_COLOR
	Depth	= 0x1801, // GL_DEPTH
	Stencil = 0x1802  // GL_STENCIL
};
PTGN_REFLECT_ENUM(ClearBufferType);

class Framebuffers {
public:
	FramebufferId CreateFramebuffer(
		std::optional<TextureId> texture = {}, Attachment texture_attachment = Attachment::Color0,
		std::optional<RenderbufferId> renderbuffer = {},
		Attachment renderbuffer_attachment = Attachment::DepthStencil, bool restore_bind = true
	);

	void DestroyFramebuffer(FramebufferId id);

	/// @brief Destroys the framebuffer and any color, depth, or stencil attachments that are
	/// attached to it.
	void DestroyFramebufferOwning(FramebufferId id);

	void AttachTexture(FramebufferId framebuffer, TextureId texture, Attachment attachment);

	void AttachRenderbuffer(
		FramebufferId framebuffer, RenderbufferId renderbuffer, Attachment attachment
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
		FramebufferId framebuffer, Color color, ClearBufferType buffer = ClearBufferType::Color,
		int drawbuffer = 0
	) const;

	/// Color -> Color
	/// Depth -> float
	/// Stencil -> uint8_t
	/// Depth+Stencil -> {float depth, uint8_t stencil}
	using PixelValue = std::variant<Color, float, std::uint8_t, std::pair<float, std::uint8_t>>;

	/// @brief WARNING: This function is slow and should be primarily used for debugging
	/// framebuffers.
	/// @param coordinate Pixel coordinate from [0, size).
	[[nodiscard]] PixelValue ReadPixel(
		FramebufferId framebuffer, V2_int coordinate, Attachment attachment = Attachment::Color0
	);

	std::vector<AttachmentSpec> GetAttachments(FramebufferId framebuffer) const;

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

	/// @brief WARNING: This function is slow and should be primarily used for debugging
	/// framebuffers.
	PixelBuffer ReadPixels(FramebufferId framebuffer, Attachment attachment = Attachment::Color0);

	/// @brief WARNING: This function is slow and should be primarily used for debugging
	/// framebuffers.
	template <typename F>
		requires std::same_as<std::invoke_result_t<F&, V2_int, PixelValue>, void>
	void ForEachPixel(
		const PixelBuffer& buffer, F&& func /* (V2_int, PixelValue) */
	) const {
		for (int y = 0; y < buffer.size.y; ++y) {
			int flipped = buffer.size.y - 1 - y;
			for (int x = 0; x < buffer.size.x; ++x) {
				int idx = flipped * buffer.size.x + x;
				PixelValue px{ DecodePixel(buffer.data, idx, buffer.type) };
				func(V2_int{ x, y }, px);
			}
		}
	}

	/// @brief WARNING: This function is slow and should be primarily used for debugging
	/// framebuffers.
	template <typename F>
		requires std::same_as<std::invoke_result_t<F&, V2_int, PixelValue>, void>
	void ForEachPixel(
		FramebufferId framebuffer, F&& func, Attachment attachment = Attachment::Color0
	) {
		PixelBuffer buffer{ ReadPixels(framebuffer, attachment) };
		ForEachPixel(buffer, std::forward<F>(func));
	}

	void SavePNG(
		const path& path, FramebufferId framebuffer, Attachment attachment = Attachment::Color0
	);

	AttachmentSpec& GetFramebufferAttachment(FramebufferId framebuffer, Attachment attachment);

	const AttachmentSpec& GetFramebufferAttachment(FramebufferId framebuffer, Attachment attachment)
		const;

	void ResizeFramebuffer(FramebufferId framebuffer, V2_int new_size);

private:
	friend class GLContext;

	explicit Framebuffers(GLContext& gl);
	~Framebuffers() noexcept						 = default;
	Framebuffers(const Framebuffers&)				 = delete;
	Framebuffers(Framebuffers&&) noexcept			 = delete;
	Framebuffers& operator=(const Framebuffers&)	 = delete;
	Framebuffers& operator=(Framebuffers&&) noexcept = delete;

	void Init(std::uint32_t max_color_attachments);

	[[nodiscard]] bool FramebufferIsComplete(FramebufferId framebuffer) const;

	const char* GetFramebufferStatus() const;

	AttachmentType GetAttachmentType(Attachment attachment) const;

	void UpdateFramebufferCache(
		FramebufferId framebuffer, std::uint32_t object_id, Attachment attachment,
		AttachmentObject object_type
	);

	[[nodiscard]] FramebufferId CreateFramebufferImpl();

	[[nodiscard]] static PixelValue DecodePixel(
		const std::vector<std::uint8_t>& data, int index, AttachmentType type
	);

	void InvalidateTexture(TextureId texture);
	void InvalidateRenderbuffer(RenderbufferId renderbuffer);

	GLContext& gl_;

	/// @brief Equivalent to GL_MAX_COLOR_ATTACHMENTS, or the number of color attachments a
	/// framebuffer can have. This is set by the Init function and should not be modified afterward.
	std::uint32_t max_color_attachments_{ 0 };

	IdMap<FramebufferCache> cache_;
};

} // namespace ptgn::impl::gl