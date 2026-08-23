#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <ostream>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "core/math/vector4.h"
#include "core/util/concepts.h"
#include "core/util/file.h"
#include "core/util/id_map.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/resources/id.h"
#include "renderer/resources/texture_format.h"

namespace ptgn::impl::gl {

class GLContext;

enum class PixelValueType : std::uint8_t {
	Color,
	FloatColor,
	Int32,
	Depth,
	Stencil,
	Depth24Stencil8,
	Depth32FStencil8
};

inline constexpr std::uint32_t kMaxColorAttachments{ 8 };

enum class Attachment : std::uint32_t {
	Color0,
	Color1,
	Color2,
	Color3,
	Color4,
	Color5,
	Color6,
	Color7,

	Depth,
	Stencil,
	DepthStencil
};

constexpr Attachment GetDepthStencilAttachment(TextureFormat format) {
	using enum Attachment;

	if (IsDepthOnlyFormat(format)) {
		return Depth;
	}
	if (IsStencilOnlyFormat(format)) {
		return Stencil;
	}

	PTGN_ASSERT(
		IsDepthStencilOnlyFormat(format), "Expected depth, stencil, or depth-stencil format"
	);

	return DepthStencil;
}

constexpr bool IsColorAttachment(Attachment attachment) noexcept {
	return attachment >= Attachment::Color0 && attachment <= Attachment::Color7;
}

[[nodiscard]] Attachment ColorAttachment(std::size_t index);

enum class AttachmentStorage : std::uint8_t {
	None,
	Texture,
	Renderbuffer
};

template <Attachment A>
struct AttachmentInfo {
	static constexpr bool is_color{ IsColorAttachment(A) };

	using Id = std::conditional_t<is_color, TextureId, RenderbufferId>;

	static constexpr AttachmentStorage storage{ is_color ? AttachmentStorage::Texture
														 : AttachmentStorage::Renderbuffer };
};

template <Attachment A>
using AttachmentIdType = typename AttachmentInfo<A>::Id;

struct FramebufferAttachment {
	Attachment attachment{ Attachment::Color0 };
	AttachmentStorage storage{ AttachmentStorage::None };
	std::uint32_t id{ 0 };

	explicit operator bool() const noexcept {
		return id != 0 && storage != AttachmentStorage::None;
	}
};

enum class ClearBufferBit : std::uint32_t {
	None	= 0,
	Color	= 1 << 0,
	Depth	= 1 << 1,
	Stencil = 1 << 2
};

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

class Framebuffers {
public:
	/// Color attachments are texture-backed by convention.
	template <Attachment A = Attachment::Color0>
	[[nodiscard]] FramebufferId Create(TextureId texture, bool restore_bind) {
		static_assert(
			IsColorAttachment(A), "Texture framebuffer attachments must be color attachments"
		);
		return CreateImpl(
			std::optional<TextureId>{ texture }, A, std::nullopt, Attachment::DepthStencil,
			restore_bind
		);
	}

	/// Depth, stencil, and depth-stencil attachments are renderbuffer-backed by convention.
	template <Attachment A = Attachment::DepthStencil>
	[[nodiscard]] FramebufferId Create(RenderbufferId renderbuffer, bool restore_bind) {
		static_assert(
			!IsColorAttachment(A),
			"Renderbuffer framebuffer attachments must not be color attachments"
		);
		return CreateImpl(
			std::nullopt, Attachment::Color0, std::optional<RenderbufferId>{ renderbuffer }, A,
			restore_bind
		);
	}

	template <
		Attachment TextureAttachment	  = Attachment::Color0,
		Attachment RenderbufferAttachment = Attachment::DepthStencil>
	[[nodiscard]] FramebufferId Create(
		TextureId texture, RenderbufferId renderbuffer, bool restore_bind
	) {
		static_assert(
			IsColorAttachment(TextureAttachment),
			"Texture framebuffer attachments must be color attachments"
		);
		static_assert(
			!IsColorAttachment(RenderbufferAttachment),
			"Renderbuffer framebuffer attachments must not be color attachments"
		);
		return CreateImpl(
			texture, TextureAttachment, renderbuffer, RenderbufferAttachment, restore_bind
		);
	}

	[[nodiscard]] FramebufferId Create(
		TextureId texture, std::optional<RenderbufferId> renderbuffer,
		Attachment renderbuffer_attachment, bool restore_bind
	);

	[[nodiscard]] FramebufferId Create(
		RenderbufferId renderbuffer, Attachment attachment, bool restore_bind
	);

	/// @brief Destroys the framebuffer and any color, depth, or stencil attachments that are
	/// attached to it.
	void Destroy(FramebufferId id, TextureId replacement_texture);

	/// @brief Destroys the framebuffer without destroying any attachments.
	/// WARNING: Use with caution, as this can lead to resource leaks if the caller does not
	/// manually destroy or reuse the attachments.
	void DestroyOnlyFramebuffer(FramebufferId id);

	template <Attachment A>
	void Attach(FramebufferId framebuffer, AttachmentIdType<A> image) {
		if constexpr (AttachmentInfo<A>::storage == AttachmentStorage::Texture) {
			AttachTextureImpl(framebuffer, image, A);
		} else {
			AttachRenderbufferImpl(framebuffer, image, A);
		}
	}

	template <Attachment A>
	void Detach(FramebufferId framebuffer) {
		Attach<A>(framebuffer, AttachmentIdType<A>{ 0 });
	}

	template <Attachment A = Attachment::Color0>
	void AttachTexture(FramebufferId framebuffer, TextureId texture) {
		static_assert(
			IsColorAttachment(A), "Texture framebuffer attachments must be color attachments"
		);
		Attach<A>(framebuffer, texture);
	}

	template <Attachment A = Attachment::DepthStencil>
	void AttachRenderbuffer(FramebufferId framebuffer, RenderbufferId renderbuffer) {
		static_assert(
			!IsColorAttachment(A),
			"Renderbuffer framebuffer attachments must not be color attachments"
		);
		Attach<A>(framebuffer, renderbuffer);
	}

	[[nodiscard]] std::optional<FramebufferAttachment> FindAttachment(
		FramebufferId framebuffer, Attachment attachment, AttachmentStorage storage
	) const;

	[[nodiscard]] bool HasAttachment(
		FramebufferId framebuffer, Attachment attachment, AttachmentStorage storage
	) const;

	[[nodiscard]] bool HasOnlyAttachmentLayout(
		FramebufferId framebuffer, std::optional<Attachment> color,
		std::optional<Attachment> depth_stencil
	) const;

	/// Clear currently bound framebuffer buffers to current OpenGL clear values.
	void Clear(
		ClearBufferBit buffers = ClearBufferBit::Color | ClearBufferBit::Stencil |
								 ClearBufferBit::Depth
	) const;

	void Clear(FramebufferId framebuffer, Color color) const {
		ClearColor(framebuffer, color);
	}

	void Clear(FramebufferId framebuffer, Depth depth) const {
		ClearDepth(framebuffer, depth);
	}

	void Clear(FramebufferId framebuffer, Stencil stencil) const {
		ClearStencil(framebuffer, stencil);
	}

	void Clear(FramebufferId framebuffer, DepthStencil depth_stencil) const {
		ClearDepthStencil(framebuffer, depth_stencil);
	}

	template <Attachment A = Attachment::Color0>
	void ClearColor(FramebufferId framebuffer, Color color) const {
		static_assert(IsColorAttachment(A), "ClearColor only supports color attachments");
		ClearColorImpl(framebuffer, A, color);
	}

	template <Attachment A>
	void ClearInt(FramebufferId framebuffer, std::int32_t value) const {
		static_assert(IsColorAttachment(A), "ClearInt only supports color attachments");
		ClearIntImpl(framebuffer, A, value);
	}

	void ClearDepth(FramebufferId framebuffer, Depth depth) const;

	void ClearStencil(FramebufferId framebuffer, Stencil stencil) const;

	void ClearDepthStencil(FramebufferId framebuffer, DepthStencil depth_stencil) const;

	using PixelValue = std::variant<Color, V4_float, std::int32_t, Depth, Stencil, DepthStencil>;

	/// @brief WARNING: This function is slow and should be primarily used for debugging
	/// framebuffers.
	/// @param coordinate Pixel coordinate from [0, size), with {0, 0} at the top-left.
	template <Attachment A = Attachment::Color0>
	[[nodiscard]] PixelValue ReadPixel(FramebufferId framebuffer, V2_int coordinate) {
		return ReadPixelImpl(framebuffer, coordinate, A);
	}

	struct PixelBuffer {
		V2_int size{};
		Attachment attachment{ Attachment::Color0 };
		PixelValueType value_type{ PixelValueType::Color };
		std::vector<std::uint8_t> data{};
	};

	/// @brief WARNING: This function is slow and should be primarily used for debugging
	/// framebuffers.
	template <Attachment A = Attachment::Color0>
	[[nodiscard]] PixelBuffer ReadPixels(FramebufferId framebuffer) {
		return ReadPixelsImpl(framebuffer, A);
	}

	/// @brief WARNING: This function is slow and should be primarily used for debugging
	/// framebuffers.
	void ForEachPixel(
		const PixelBuffer& buffer, InvocableR<void, V2_int, PixelValue> auto func
	) const {
		PTGN_ASSERT(buffer.size.IsPositive(), "Cannot loop through buffer with invalid size: ", buffer.size);
		
		for (int y{ 0 }; y < buffer.size.y; ++y) {
			const int flipped_y{ buffer.size.y - 1 - y };

			for (int x{ 0 }; x < buffer.size.x; ++x) {
				const int index{ flipped_y * buffer.size.x + x };
				PixelValue pixel{ DecodePixel(buffer.data, index, buffer.value_type) };

				std::invoke(func, V2_int{ x, y }, pixel);
			}
		}
	}

	/// @brief WARNING: This function is slow and should be primarily used for debugging
	/// framebuffers.
	template <Attachment A = Attachment::Color0, InvocableR<void, V2_int, PixelValue> F>
	void ForEachPixel(FramebufferId framebuffer, F&& func) {
		PixelBuffer buffer{ ReadPixels<A>(framebuffer) };
		ForEachPixel(buffer, std::forward<F>(func));
	}

	template <Attachment A = Attachment::Color0>
	void SavePNG(const path& path, FramebufferId framebuffer) {
		static_assert(IsColorAttachment(A), "SavePNG only supports color attachments");
		SavePNGImpl(path, framebuffer, A);
	}

	[[nodiscard]] std::vector<FramebufferAttachment> GetAttachments(
		FramebufferId framebuffer
	) const;

	template <Attachment A = Attachment::Color0>
	[[nodiscard]] FramebufferAttachment GetAttachmentInfo(FramebufferId framebuffer) const {
		auto info{ GetAttachmentInfoImpl(framebuffer, A) };

		if (info.id != 0) {
			PTGN_ASSERT(
				info.storage == AttachmentInfo<A>::storage,
				"Framebuffer attachment does not match the expected storage type"
			);
		}

		return info;
	}

	template <Attachment A = Attachment::Color0>
	[[nodiscard]] AttachmentIdType<A> GetAttachment(FramebufferId framebuffer) const {
		auto info{ GetAttachmentInfo<A>(framebuffer) };

		PTGN_ASSERT(info.id != 0, "Framebuffer attachment must be valid");

		return AttachmentIdType<A>{ info.id };
	}

	template <Attachment A = Attachment::Color0>
	[[nodiscard]] AttachmentIdType<A> GetAttachmentId(FramebufferId framebuffer) const {
		return GetAttachment<A>(framebuffer);
	}

	template <Attachment A>
	[[nodiscard]] bool HasAttachment(FramebufferId framebuffer) const {
		auto info{ GetAttachmentInfo<A>(framebuffer) };
		return info.id && info.storage != AttachmentStorage::None;
	}

	void Resize(FramebufferId framebuffer, V2_int new_size);

	template <Attachment A = Attachment::Color0>
	void CopyRegion(
		FramebufferId source, FramebufferId destination, Viewport source_region,
		V2_int destination_position
	) const {
		CopyRegionImpl(source, destination, A, source_region, destination_position);
	}

private:
	friend class GLContext;

	struct AttachmentRecord {
		std::uint32_t id{ 0 };
		AttachmentStorage storage{ AttachmentStorage::None };
	};

	struct FramebufferCache {
		std::array<AttachmentRecord, kMaxColorAttachments> color{};
		AttachmentRecord depth{};
		AttachmentRecord stencil{};
		AttachmentRecord depth_stencil{};
	};

	explicit Framebuffers(GLContext& gl);
	~Framebuffers() noexcept						 = default;
	Framebuffers(const Framebuffers&)				 = delete;
	Framebuffers(Framebuffers&&) noexcept			 = delete;
	Framebuffers& operator=(const Framebuffers&)	 = delete;
	Framebuffers& operator=(Framebuffers&&) noexcept = delete;

	TextureFormat GetAttachmentFormat(const AttachmentRecord& record) const;
	void UpdateDrawBuffers(FramebufferId framebuffer) const;

	[[nodiscard]] FramebufferId CreateImpl(
		std::optional<TextureId> texture, Attachment texture_attachment,
		std::optional<RenderbufferId> renderbuffer, Attachment renderbuffer_attachment,
		bool restore_bind
	);

	void AttachTextureImpl(FramebufferId framebuffer, TextureId texture, Attachment attachment);

	void AttachRenderbufferImpl(
		FramebufferId framebuffer, RenderbufferId renderbuffer, Attachment attachment
	);

	void ClearColorImpl(FramebufferId framebuffer, Attachment attachment, Color color) const;
	void ClearIntImpl(FramebufferId framebuffer, Attachment attachment, std::int32_t value) const;

	[[nodiscard]] PixelValue ReadPixelImpl(
		FramebufferId framebuffer, V2_int coordinate, Attachment attachment
	);

	[[nodiscard]] PixelBuffer ReadPixelsImpl(FramebufferId framebuffer, Attachment attachment);

	void SavePNGImpl(const path& path, FramebufferId framebuffer, Attachment attachment);

	[[nodiscard]] FramebufferAttachment GetAttachmentInfoImpl(
		FramebufferId framebuffer, Attachment attachment
	) const;

	[[nodiscard]] const AttachmentRecord& GetAttachmentRecord(
		FramebufferId framebuffer, Attachment attachment
	) const;

	[[nodiscard]] AttachmentRecord& GetAttachmentRecord(
		FramebufferId framebuffer, Attachment attachment
	);

	void UpdateCache(
		FramebufferId framebuffer, Attachment attachment, std::uint32_t id,
		AttachmentStorage storage
	);

	[[nodiscard]] V2_int GetAttachmentSize(const AttachmentRecord& record) const;

	[[nodiscard]] bool IsComplete(FramebufferId framebuffer) const;

	[[nodiscard]] const char* GetStatus() const;

	void CopyRegionImpl(
		FramebufferId source, FramebufferId destination, Attachment attachment,
		Viewport source_region, V2_int destination_position
	) const;

	[[nodiscard]] FramebufferId CreateBareFramebuffer();

	[[nodiscard]] static PixelValue DecodePixel(
		const std::vector<std::uint8_t>& data, int index, PixelValueType value_type
	);

	void InvalidateTexture(TextureId texture);
	void InvalidateRenderbuffer(RenderbufferId renderbuffer);

	GLContext& gl_;

	IdMap<FramebufferCache> cache_;
};

} // namespace ptgn::impl::gl