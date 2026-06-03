#include "renderer/backend/gl/gl_framebuffer.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <expected>
#include <filesystem>
#include <optional>
#include <ostream>
#include <utility>
#include <variant>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/graphics/surface.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "core/math/vector4.h"
#include "core/util/file.h"
#include "core/util/id_map.h"
#include "renderer/backend/gl/gl.h"
#include "renderer/backend/gl/gl_context.h"
#include "renderer/backend/gl/gl_renderbuffer.h"
#include "renderer/backend/gl/gl_state.h"
#include "renderer/backend/gl/gl_texture.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/resources/id.h"

namespace ptgn::impl::gl {

namespace {

[[nodiscard]] std::size_t ColorAttachmentIndex(Attachment attachment) {
	PTGN_ASSERT(IsColorAttachment(attachment), "Attachment is not a color attachment");
	return static_cast<std::size_t>(
		std::to_underlying(attachment) - std::to_underlying(Attachment::Color0)
	);
}

[[nodiscard]] GLenum ToGLAttachment(Attachment attachment) {
	if (IsColorAttachment(attachment)) {
		return static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + ColorAttachmentIndex(attachment));
	}

	switch (attachment) {
		using enum Attachment;
		case Depth:		   return GL_DEPTH_ATTACHMENT;
		case Stencil:	   return GL_STENCIL_ATTACHMENT;
		case DepthStencil: return GL_DEPTH_STENCIL_ATTACHMENT;
		default:		   PTGN_ERROR("Unknown Attachment: ", std::to_underlying(attachment));
	}
}

[[nodiscard]] AttachmentStorage ExpectedStorage(Attachment attachment) {
	return IsColorAttachment(attachment) ? AttachmentStorage::Texture
										 : AttachmentStorage::Renderbuffer;
}

[[nodiscard]] GLbitfield ToGLClearMask(ClearBufferBit buffers) {
	using enum ClearBufferBit;

	GLbitfield mask{ 0 };

	if ((buffers & Color) == Color) {
		mask |= GL_COLOR_BUFFER_BIT;
	}

	if ((buffers & Depth) == Depth) {
		mask |= GL_DEPTH_BUFFER_BIT;
	}

	if ((buffers & Stencil) == Stencil) {
		mask |= GL_STENCIL_BUFFER_BIT;
	}

	return mask;
}

struct ReadSpec {
	GLenum format{ GL_RGBA };
	GLenum type{ GL_UNSIGNED_BYTE };
	std::size_t bytes_per_pixel{ 4 };
};

[[nodiscard]] ReadSpec GetReadSpec(Attachment attachment) {
	if (IsColorAttachment(attachment)) {
		return ReadSpec{ GL_RGBA, GL_UNSIGNED_BYTE, 4 };
	}

	switch (attachment) {
		using enum Attachment;
		case Depth:	  return ReadSpec{ GL_DEPTH_COMPONENT, GL_FLOAT, sizeof(float) };
		case Stencil: return ReadSpec{ GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, 1 };
		case DepthStencil:
			return ReadSpec{ GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, sizeof(std::uint32_t) };
		default: PTGN_ERROR("Unknown Attachment: ", std::to_underlying(attachment));
	}
}

void ReadPixelsRaw(V2_int coord, V2_int size, const ReadSpec& read_spec, void* data) {
	GLCall(glReadPixels(coord.x, coord.y, size.x, size.y, read_spec.format, read_spec.type, data));
}

void SelectReadBufferIfColor(Attachment attachment, std::optional<GLint>& previous) {
	if (!IsColorAttachment(attachment)) {
		return;
	}

	GLint previous_read_buffer{ 0 };
	GLCall(glGetIntegerv(GL_READ_BUFFER, &previous_read_buffer));

	previous = previous_read_buffer;

	GLCall(glReadBuffer(ToGLAttachment(attachment)));
}

void RestoreReadBuffer(const std::optional<GLint>& previous) {
	if (!previous.has_value()) {
		return;
	}

	GLCall(glReadBuffer(static_cast<GLenum>(*previous)));
}

void SelectDrawBufferIfColor(Attachment attachment, std::optional<GLint>& previous) {
	if (!IsColorAttachment(attachment)) {
		return;
	}

	GLint previous_draw_buffer{ 0 };
	GLCall(glGetIntegerv(GL_DRAW_BUFFER0, &previous_draw_buffer));

	previous = previous_draw_buffer;

	GLCall(glDrawBuffer(ToGLAttachment(attachment)));
}

void RestoreDrawBuffer(const std::optional<GLint>& previous) {
	if (!previous.has_value()) {
		return;
	}

	GLCall(glDrawBuffer(static_cast<GLenum>(*previous)));
}

[[nodiscard]] GLbitfield BlitMask(Attachment attachment) {
	if (IsColorAttachment(attachment)) {
		return GL_COLOR_BUFFER_BIT;
	}

	switch (attachment) {
		using enum Attachment;
		case Depth:		   return GL_DEPTH_BUFFER_BIT;
		case Stencil:	   return GL_STENCIL_BUFFER_BIT;
		case DepthStencil: return GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
		default:		   PTGN_ERROR("Unknown Attachment: ", std::to_underlying(attachment));
	}
}

} // namespace

Attachment ColorAttachment(std::size_t index) {
	PTGN_ASSERT(index < kMaxColorAttachments, "Color attachment out of range");
	return static_cast<Attachment>(std::to_underlying(Attachment::Color0) + index);
}

Framebuffers::Framebuffers(GLContext& gl) : gl_{ gl } {}

FramebufferId Framebuffers::CreateImpl(
	std::optional<TextureId> texture, Attachment texture_attachment,
	std::optional<RenderbufferId> renderbuffer, Attachment renderbuffer_attachment,
	bool restore_bind
) {
	PTGN_ASSERT(
		texture.has_value() || renderbuffer.has_value(),
		"Must provide at least one valid image attachment when creating a framebuffer"
	);

	auto framebuffer{ CreateBareFramebuffer() };
	auto _{ gl_.Bind(framebuffer, restore_bind) };

	if (texture.has_value()) {
		AttachTextureImpl(framebuffer, *texture, texture_attachment);
	}

	if (renderbuffer.has_value()) {
		AttachRenderbufferImpl(framebuffer, *renderbuffer, renderbuffer_attachment);
	}

	PTGN_ASSERT(IsComplete(framebuffer), "Framebuffer is incomplete: ", GetStatus());
	PTGN_ASSERT(
		GLCallReturn(glIsFramebuffer(framebuffer)), "Failed to create a valid OpenGL framebuffer"
	);

	return framebuffer;
}

void Framebuffers::AttachTextureImpl(
	FramebufferId framebuffer, TextureId texture, Attachment attachment
) {
	PTGN_ASSERT(IsColorAttachment(attachment), "Textures are only used for color attachments");
	PTGN_ASSERT(gl_.IsBound(framebuffer), "FramebufferId must be bound before attaching a texture");

	if (texture) {
		PTGN_ASSERT(
			gl_.textures.GetDesc(texture).size.IsPositive(), "Cannot attach a texture with no size"
		);
	}

	PTGN_ASSERT(
		GLCallReturn(glIsFramebuffer(framebuffer)),
		"FramebufferId is not a valid OpenGL framebuffer"
	);
	PTGN_ASSERT(
		!texture || GLCallReturn(glIsTexture(texture)), "TextureId is not a valid OpenGL texture"
	);

	constexpr std::int32_t mipmap_level{ 0 };

	GLCall(glFramebufferTexture2D(
		GL_FRAMEBUFFER, ToGLAttachment(attachment), GL_TEXTURE_2D, texture.value, mipmap_level
	));

	UpdateCache(
		framebuffer, attachment, texture.value,
		texture ? AttachmentStorage::Texture : AttachmentStorage::None
	);
}

void Framebuffers::AttachRenderbufferImpl(
	FramebufferId framebuffer, RenderbufferId renderbuffer, Attachment attachment
) {
	PTGN_ASSERT(
		!IsColorAttachment(attachment),
		"Renderbuffers are only used for depth, stencil, or depth-stencil attachments"
	);
	PTGN_ASSERT(
		gl_.IsBound(framebuffer), "FramebufferId must be bound before attaching a renderbuffer"
	);

	if (renderbuffer) {
		PTGN_ASSERT(
			gl_.renderbuffers.GetCache(renderbuffer).size.IsPositive(),
			"Cannot attach a renderbuffer with no size"
		);
	}

	PTGN_ASSERT(
		GLCallReturn(glIsFramebuffer(framebuffer)),
		"FramebufferId is not a valid OpenGL framebuffer"
	);
	PTGN_ASSERT(
		!renderbuffer || GLCallReturn(glIsRenderbuffer(renderbuffer)),
		"RenderbufferId is not a valid OpenGL renderbuffer"
	);

	GLCall(glFramebufferRenderbuffer(
		GL_FRAMEBUFFER, ToGLAttachment(attachment), GL_RENDERBUFFER, renderbuffer.value
	));

	UpdateCache(
		framebuffer, attachment, renderbuffer.value,
		renderbuffer ? AttachmentStorage::Renderbuffer : AttachmentStorage::None
	);
}

void Framebuffers::Clear(ClearBufferBit buffers) const {
	GLCall(glClear(ToGLClearMask(buffers)));
}

void Framebuffers::ClearColorImpl(
	FramebufferId framebuffer, Attachment attachment, Color color
) const {
	PTGN_ASSERT(gl_.IsBound(framebuffer), "FramebufferId must be bound before clearing color");
	PTGN_ASSERT(IsColorAttachment(attachment), "ClearColor only supports color attachments");

	std::optional<GLint> previous_draw_buffer;
	SelectDrawBufferIfColor(attachment, previous_draw_buffer);

	auto c{ static_cast<V4_float>(color) };
	GLCall(glClearBufferfv(GL_COLOR, 0, c.Data()));

	RestoreDrawBuffer(previous_draw_buffer);
}

void Framebuffers::ClearDepth(FramebufferId framebuffer, Depth depth) const {
	PTGN_ASSERT(gl_.IsBound(framebuffer), "FramebufferId must be bound before clearing depth");
	GLCall(glClearBufferfv(GL_DEPTH, 0, &depth.value));
}

void Framebuffers::ClearStencil(FramebufferId framebuffer, Stencil stencil) const {
	PTGN_ASSERT(gl_.IsBound(framebuffer), "FramebufferId must be bound before clearing stencil");

	GLCall(glClearBufferiv(GL_STENCIL, 0, &stencil.value));
}

void Framebuffers::ClearDepthStencil(FramebufferId framebuffer, DepthStencil depth_stencil) const {
	PTGN_ASSERT(
		gl_.IsBound(framebuffer), "FramebufferId must be bound before clearing depth stencil"
	);
	GLCall(
		glClearBufferfi(GL_DEPTH_STENCIL, 0, depth_stencil.depth.value, depth_stencil.stencil.value)
	);
}

Framebuffers::PixelValue Framebuffers::ReadPixelImpl(
	FramebufferId framebuffer, V2_int coordinate, Attachment attachment
) {
	PTGN_ASSERT(gl_.IsBound(framebuffer), "FramebufferId must be bound before reading pixel");

	const auto& record{ GetAttachmentRecord(framebuffer, attachment) };

	PTGN_ASSERT(record.id != 0, "No image attached to framebuffer attachment");
	PTGN_ASSERT(
		record.storage == ExpectedStorage(attachment),
		"Framebuffer attachment does not match the expected storage type"
	);

	auto size{ GetAttachmentSize(record) };

	PTGN_ASSERT(
		coordinate.x >= 0 && coordinate.x < size.x,
		"Cannot read pixel outside framebuffer attachment bounds"
	);
	PTGN_ASSERT(
		coordinate.y >= 0 && coordinate.y < size.y,
		"Cannot read pixel outside framebuffer attachment bounds"
	);

	V2_int read_coord{ coordinate.x, size.y - 1 - coordinate.y };
	constexpr V2_int query_size{ 1, 1 };

	auto read_spec{ GetReadSpec(attachment) };
	std::vector<std::uint8_t> data(read_spec.bytes_per_pixel);

	std::optional<GLint> previous_read_buffer;
	SelectReadBufferIfColor(attachment, previous_read_buffer);

	ReadPixelsRaw(read_coord, query_size, read_spec, data.data());

	RestoreReadBuffer(previous_read_buffer);

	return DecodePixel(data, 0, attachment);
}

Framebuffers::PixelBuffer Framebuffers::ReadPixelsImpl(
	FramebufferId framebuffer, Attachment attachment
) {
	PTGN_ASSERT(gl_.IsBound(framebuffer), "FramebufferId must be bound before reading pixels");

	const auto& record{ GetAttachmentRecord(framebuffer, attachment) };

	PTGN_ASSERT(record.id != 0, "No image attached to framebuffer attachment");
	PTGN_ASSERT(
		record.storage == ExpectedStorage(attachment),
		"Framebuffer attachment does not match the expected storage type"
	);

	auto size{ GetAttachmentSize(record) };
	PTGN_ASSERT(size.IsPositive(), "Cannot read pixels from an attachment with no size");

	auto read_spec{ GetReadSpec(attachment) };

	auto pixel_count{ static_cast<std::size_t>(size.x) * static_cast<std::size_t>(size.y) };
	std::vector<std::uint8_t> data(pixel_count * read_spec.bytes_per_pixel);

	std::optional<GLint> previous_read_buffer;
	SelectReadBufferIfColor(attachment, previous_read_buffer);

	ReadPixelsRaw({ 0, 0 }, size, read_spec, data.data());

	RestoreReadBuffer(previous_read_buffer);

	return PixelBuffer{ .size = size, .attachment = attachment, .data = std::move(data) };
}

std::vector<FramebufferAttachment> Framebuffers::GetAttachments(FramebufferId framebuffer) const {
	std::vector<FramebufferAttachment> attachments;

	if (!framebuffer) {
		return attachments;
	}

	PTGN_ASSERT(cache_.Has(framebuffer), "No framebuffer with id ", framebuffer, " in cache");

	const auto& cache{ cache_.Get(framebuffer) };

	for (auto i{ 0uz }; i < cache.color.size(); ++i) {
		if (cache.color[i].id != 0) {
			attachments.push_back(
				FramebufferAttachment{
					.attachment = ColorAttachment(i),
					.storage	= cache.color[i].storage,
					.id			= cache.color[i].id,
				}
			);
		}
	}

	if (cache.depth.id != 0) {
		attachments.push_back(
			FramebufferAttachment{
				.attachment = Attachment::Depth,
				.storage	= cache.depth.storage,
				.id			= cache.depth.id,
			}
		);
	}

	if (cache.stencil.id != 0) {
		attachments.push_back(
			FramebufferAttachment{
				.attachment = Attachment::Stencil,
				.storage	= cache.stencil.storage,
				.id			= cache.stencil.id,
			}
		);
	}

	if (cache.depth_stencil.id != 0) {
		attachments.push_back(
			FramebufferAttachment{
				.attachment = Attachment::DepthStencil,
				.storage	= cache.depth_stencil.storage,
				.id			= cache.depth_stencil.id,
			}
		);
	}

	return attachments;
}

FramebufferAttachment Framebuffers::GetAttachmentInfoImpl(
	FramebufferId framebuffer, Attachment attachment
) const {
	const auto& record{ GetAttachmentRecord(framebuffer, attachment) };

	return FramebufferAttachment{
		.attachment = attachment,
		.storage	= record.storage,
		.id			= record.id,
	};
}

const Framebuffers::AttachmentRecord& Framebuffers::GetAttachmentRecord(
	FramebufferId framebuffer, Attachment attachment
) const {
	PTGN_ASSERT(cache_.Has(framebuffer), "No framebuffer with id ", framebuffer, " in cache");

	const auto& cache{ cache_.Get(framebuffer) };

	if (IsColorAttachment(attachment)) {
		return cache.color[ColorAttachmentIndex(attachment)];
	}

	switch (attachment) {
		using enum Attachment;
		case Depth:		   return cache.depth;
		case Stencil:	   return cache.stencil;
		case DepthStencil: return cache.depth_stencil;
		default:		   PTGN_ERROR("Unknown Attachment: ", std::to_underlying(attachment));
	}
}

Framebuffers::AttachmentRecord& Framebuffers::GetAttachmentRecord(
	FramebufferId framebuffer, Attachment attachment
) {
	return const_cast<AttachmentRecord&>( // NOSONAR
		std::as_const(*this).GetAttachmentRecord(framebuffer, attachment)
	);
}

void Framebuffers::UpdateCache(
	FramebufferId framebuffer, Attachment attachment, std::uint32_t id, AttachmentStorage storage
) {
	auto& record{ GetAttachmentRecord(framebuffer, attachment) };

	record.id	   = id;
	record.storage = id != 0 ? storage : AttachmentStorage::None;
}

V2_int Framebuffers::GetAttachmentSize(const AttachmentRecord& record) const {
	PTGN_ASSERT(record.id != 0, "Cannot query the size of an empty framebuffer attachment");

	switch (record.storage) {
		using enum AttachmentStorage;
		case Texture:	   return gl_.textures.GetDesc(TextureId{ record.id }).size;
		case Renderbuffer: return gl_.renderbuffers.GetCache(RenderbufferId{ record.id }).size;
		case None:		   [[fallthrough]];
		default:		   PTGN_ERROR("Cannot query the size of an empty framebuffer attachment");
	}
}

bool Framebuffers::IsComplete(FramebufferId framebuffer) const {
	PTGN_ASSERT(gl_.IsBound(framebuffer), "Cannot check status of framebuffer until it is bound");

	auto status{ GLCallReturn(glCheckFramebufferStatus(GL_FRAMEBUFFER)) };

	return status == GL_FRAMEBUFFER_COMPLETE;
}

const char* Framebuffers::GetStatus() const {
	auto status{ GLCallReturn(glCheckFramebufferStatus(GL_FRAMEBUFFER)) };

	switch (status) {
		case GL_FRAMEBUFFER_COMPLETE:			   return "GL_FRAMEBUFFER_COMPLETE";
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT: return "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
			return "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
		case GL_FRAMEBUFFER_UNSUPPORTED:			return "GL_FRAMEBUFFER_UNSUPPORTED";
		case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE: return "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE";

#ifdef GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER
		case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER: return "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER";
#endif

#ifdef GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER
		case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER: return "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER";
#endif

#ifdef GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS
		case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
			return "GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS";
#endif

		default: PTGN_ERROR("Unknown framebuffer status: ", status);
	}
}

void Framebuffers::CopyRegionImpl(
	FramebufferId source, FramebufferId destination, Attachment attachment, Viewport source_region,
	V2_int destination_position
) const {
	PTGN_ASSERT(source, "Source framebuffer must be valid");
	PTGN_ASSERT(destination, "Destination framebuffer must be valid");
	PTGN_ASSERT(
		!source_region.position.IsNegative() && source_region.size.IsPositive(),
		"Source framebuffer copy region must be valid"
	);

	PTGN_ASSERT(cache_.Has(source), "Source framebuffer must be in the cache");
	PTGN_ASSERT(cache_.Has(destination), "Destination framebuffer must be in the cache");

	auto source_attachment{ GetAttachmentInfoImpl(source, attachment) };
	auto destination_attachment{ GetAttachmentInfoImpl(destination, attachment) };

	PTGN_ASSERT(source_attachment, "Source framebuffer attachment must be valid");
	PTGN_ASSERT(destination_attachment, "Destination framebuffer attachment must be valid");
	PTGN_ASSERT(
		source_attachment.storage == ExpectedStorage(attachment),
		"Source framebuffer attachment does not match the expected storage type"
	);
	PTGN_ASSERT(
		destination_attachment.storage == ExpectedStorage(attachment),
		"Destination framebuffer attachment does not match the expected storage type"
	);

	PTGN_ASSERT(GLCallReturn(glIsFramebuffer(source)), "Source is not a valid OpenGL framebuffer");
	PTGN_ASSERT(
		GLCallReturn(glIsFramebuffer(destination)), "Destination is not a valid OpenGL framebuffer"
	);

	auto previous_framebuffer{ gl_.GetBoundFramebuffer() };

	PTGN_ASSERT(gl_.ScissorCoversFramebuffer(source));

	GLCall(glBindFramebuffer(GL_READ_FRAMEBUFFER, source));
	std::optional<GLint> previous_read_buffer;
	SelectReadBufferIfColor(attachment, previous_read_buffer);

	GLCall(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, destination));
	std::optional<GLint> previous_draw_buffer;
	SelectDrawBufferIfColor(attachment, previous_draw_buffer);

	auto src_min{ source_region.position };
	auto src_max{ source_region.position + source_region.size };

	auto dst_min{ destination_position };
	auto dst_max{ destination_position + source_region.size };

	GLCall(glBlitFramebuffer(
		src_min.x, src_min.y, src_max.x, src_max.y, dst_min.x, dst_min.y, dst_max.x, dst_max.y,
		BlitMask(attachment), GL_NEAREST
	));

	RestoreReadBuffer(previous_read_buffer);
	RestoreDrawBuffer(previous_draw_buffer);

	if (previous_framebuffer.has_value()) {
		// Guarantee that a new framebuffer is bound after, since gl_.Bind will not recognize
		// GL_READ_FRAMEBUFFER or GL_DRAW_FRAMEBUFFER changes.
		gl_.bound_.framebuffer = std::nullopt;
		auto _				   = gl_.Bind(*previous_framebuffer, false);
	}
}

void Framebuffers::Resize(FramebufferId framebuffer, V2_int new_size) {
	const auto& cache{ cache_.Get(framebuffer) };

	auto resize_attachment = [this, new_size](const AttachmentRecord& record) {
		if (record.id == 0) {
			return;
		}

		switch (record.storage) {
			using enum AttachmentStorage;
			case Texture: gl_.textures.Resize(TextureId{ record.id }, new_size); return;
			case Renderbuffer:
				gl_.renderbuffers.Resize(RenderbufferId{ record.id }, new_size);
				return;
			case None: [[fallthrough]];
			default:
				PTGN_ERROR(
					"Unknown or None AttachmentStorage: ", std::to_underlying(record.storage)
				);
		}
	};

	for (const auto& color : cache.color) {
		resize_attachment(color);
	}

	resize_attachment(cache.depth);
	resize_attachment(cache.stencil);
	resize_attachment(cache.depth_stencil);
}

FramebufferId Framebuffers::CreateBareFramebuffer() {
	FramebufferId id{ 0 };

	GLCall(glGenFramebuffers(1, &id.value));

	PTGN_ASSERT(id, "Failed to create framebuffer");

	cache_.Add(id, FramebufferCache{});

	return id;
}

Framebuffers::PixelValue Framebuffers::DecodePixel(
	const std::vector<std::uint8_t>& data, int index, Attachment attachment
) {
	PTGN_ASSERT(index >= 0, "Pixel index cannot be negative");

	if (IsColorAttachment(attachment)) {
		std::size_t offset{ static_cast<std::size_t>(index) * 4 };

		PTGN_ASSERT(offset + 3 < data.size(), "Pixel buffer does not contain an RGBA pixel");

		return Color{ data[offset + 0], data[offset + 1], data[offset + 2], data[offset + 3] };
	}

	switch (attachment) {
		using enum Attachment;
		case Depth: {
			std::size_t offset{ static_cast<std::size_t>(index) * sizeof(float) };

			PTGN_ASSERT(
				offset + sizeof(float) <= data.size(), "Pixel buffer does not contain depth"
			);

			float depth{ 0.0f };
			std::memcpy(&depth, data.data() + offset, sizeof(depth));

			return ptgn::Depth{ depth };
		}

		case Stencil: {
			std::size_t offset{ static_cast<std::size_t>(index) };

			PTGN_ASSERT(offset < data.size(), "Pixel buffer does not contain stencil");

			return ptgn::Stencil{ data[offset] };
		}

		case DepthStencil: {
			std::size_t offset{ static_cast<std::size_t>(index) * sizeof(std::uint32_t) };

			PTGN_ASSERT(
				offset + sizeof(std::uint32_t) <= data.size(),
				"Pixel buffer does not contain packed depth-stencil"
			);

			std::uint32_t packed{ 0 };
			std::memcpy(&packed, data.data() + offset, sizeof(packed));

			constexpr std::uint32_t kDepthMax{ 0x00FFFFFFu };

			auto depth_bits{ packed >> 8 };
			float depth{ static_cast<float>(depth_bits) / static_cast<float>(kDepthMax) };
			auto stencil{ static_cast<std::uint8_t>(packed & 0xFFu) };

			return ptgn::DepthStencil{ .depth{ depth }, .stencil{ stencil } };
		}

		default: PTGN_ERROR("Unknown framebuffer attachment: ", std::to_underlying(attachment));
	}
}

void Framebuffers::InvalidateTexture(TextureId texture) {
	for (auto item : cache_.Items()) {
		FramebufferId framebuffer{ static_cast<std::uint32_t>(item.id) };

		std::array<Attachment, kMaxColorAttachments> pending{};
		std::size_t count{ 0 };

		for (std::size_t i{ 0 }; i < item.value.color.size(); ++i) {
			auto& attachment{ item.value.color[i] };

			if (attachment.id == texture.value &&
				attachment.storage == AttachmentStorage::Texture) {
				attachment = {};
				PTGN_ASSERT(count < pending.size());
				pending[count] = ColorAttachment(i);
				count++;
			}
		}

		if (count == 0) {
			continue;
		}

		auto _{ gl_.Bind(framebuffer, true) };

		for (auto i{ 0uz }; i < count; ++i) {
			AttachTextureImpl(framebuffer, TextureId{ 0 }, pending[i]);
		}
	}
}

void Framebuffers::InvalidateRenderbuffer(RenderbufferId renderbuffer) {
	for (auto item : cache_.Items()) {
		FramebufferId framebuffer{ static_cast<std::uint32_t>(item.id) };

		constexpr std::size_t kMaxDepthStencilAttachments{ 3 };
		std::array<Attachment, kMaxDepthStencilAttachments> pending{};
		std::size_t count{ 0 };

		auto queue_if_matching = [&](AttachmentRecord& attachment, Attachment slot) {
			if (attachment.id != renderbuffer.value ||
				attachment.storage != AttachmentStorage::Renderbuffer) {
				return;
			}

			attachment = {};
			PTGN_ASSERT(count < pending.size());
			pending[count] = slot;
			count++;
		};

		queue_if_matching(item.value.depth, Attachment::Depth);
		queue_if_matching(item.value.stencil, Attachment::Stencil);
		queue_if_matching(item.value.depth_stencil, Attachment::DepthStencil);

		if (count == 0) {
			continue;
		}

		auto _{ gl_.Bind(framebuffer, true) };

		for (auto i{ 0uz }; i < count; ++i) {
			AttachRenderbufferImpl(framebuffer, RenderbufferId{ 0 }, pending[i]);
		}
	}
}

void Framebuffers::Destroy(FramebufferId id) {
	if (!cache_.Has(id)) {
		DestroyOnlyFramebuffer(id);
		return;
	}

	std::vector<FramebufferAttachment> destroyed;

	for (const auto& attachment : GetAttachments(id)) {
		PTGN_ASSERT(attachment.id != 0);

		bool already_destroyed{ false };

		for (const auto& existing : destroyed) {
			if (existing.id == attachment.id && existing.storage == attachment.storage) {
				already_destroyed = true;
				break;
			}
		}

		if (already_destroyed) {
			continue;
		}

		destroyed.push_back(attachment);

		switch (attachment.storage) {
			using enum AttachmentStorage;
			case Texture:	   gl_.textures.Destroy(TextureId{ attachment.id }); break;
			case Renderbuffer: gl_.renderbuffers.Destroy(RenderbufferId{ attachment.id }); break;
			case None:		   PTGN_ERROR("Cannot destroy an empty framebuffer attachment");
		}
	}

	DestroyOnlyFramebuffer(id);
}

void Framebuffers::DestroyOnlyFramebuffer(FramebufferId id) {
	if (!id) {
		return;
	}

	gl_.ForgetId(id);

	PTGN_ASSERT(!gl_.IsBound(id), "FramebufferId must not be bound when destroying it");

	GLCall(glDeleteFramebuffers(1, &id.value));

	cache_.Remove(id);
}

void Framebuffers::SavePNGImpl(const path& path, FramebufferId framebuffer, Attachment attachment) {
	PTGN_ASSERT(IsColorAttachment(attachment), "SavePNG only supports color attachments");

	if (path.has_parent_path()) {
		std::filesystem::create_directories(path.parent_path());
	}

	PixelBuffer buffer{ ReadPixelsImpl(framebuffer, attachment) };

	auto size{ buffer.size };
	constexpr int channels{ 4 };

	std::vector<std::uint8_t> rgba(static_cast<std::size_t>(size.x) * size.y * channels);

	ForEachPixel(buffer, [&rgba, size](V2_int pos, const PixelValue& pixel) {
		const auto* color{ std::get_if<Color>(&pixel) };
		PTGN_ASSERT(color);

		std::size_t index{ static_cast<std::size_t>(pos.y * size.x + pos.x) * channels };

		rgba[index + 0] = color->r;
		rgba[index + 1] = color->g;
		rgba[index + 2] = color->b;
		rgba[index + 3] = color->a;
	});

	impl::Surface surface{ size, rgba, channels };

	auto success{ surface.SavePNG(path) };

	PTGN_ASSERT(success.has_value(), success.error());
}

std::ostream& operator<<(std::ostream& os, ClearBufferBit bits) {
	if (bits == ClearBufferBit::None) {
		return os << "None";
	}

	bool first{ true };

	auto print_flag = [&](ClearBufferBit flag, const char* name) {
		if ((bits & flag) != flag) {
			return;
		}

		if (!first) {
			os << " | ";
		}

		os << name;
		first = false;
	};

	print_flag(ClearBufferBit::Color, "Color");
	print_flag(ClearBufferBit::Depth, "Depth");
	print_flag(ClearBufferBit::Stencil, "Stencil");

	return os;
}

} // namespace ptgn::impl::gl