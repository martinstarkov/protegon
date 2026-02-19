#include "renderer/backend/gl/gl_framebuffer.h"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_surface.h>
#include <SDL3_image/SDL_image.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "core/math/vector4.h"
#include "core/util/file.h"
#include "core/util/id_map.h"
#include "renderer/backend/gl/gl.h"
#include "renderer/backend/gl/gl_context.h"
#include "renderer/backend/gl/gl_renderbuffer.h"
#include "renderer/backend/gl/gl_texture.h"
#include "renderer/resources/framebuffer.h"
#include "renderer/resources/id.h"
#include "renderer/resources/renderbuffer.h"
#include "renderer/resources/texture.h"

namespace ptgn::impl::gl {

Framebuffers::Framebuffers(GLContext& gl) : gl_{ gl } {}

void Framebuffers::Init(std::uint32_t max_color_attachments) {
	max_color_attachments_ = max_color_attachments;
}

Framebuffer Framebuffers::CreateFramebuffer(
	std::optional<Texture> texture, Attachment texture_attachment,
	std::optional<Renderbuffer> renderbuffer, Attachment renderbuffer_attachment, bool restore_bind
) {
	PTGN_ASSERT(
		texture.has_value() || renderbuffer.has_value(),
		"Must provide at least one valid image attachment when creating a framebuffer"
	);

	auto framebuffer{ CreateFramebufferImpl() };
	auto _ = gl_.Bind(framebuffer, restore_bind);

	if (texture.has_value()) {
		AttachTexture(framebuffer, *texture, texture_attachment);
	}

	if (renderbuffer.has_value()) {
		AttachRenderbuffer(framebuffer, *renderbuffer, renderbuffer_attachment);
	}

	PTGN_ASSERT(FramebufferIsComplete(framebuffer));

	return framebuffer;
}

void Framebuffers::AttachTexture(Framebuffer framebuffer, Texture texture, Attachment attachment) {
	PTGN_ASSERT(gl_.IsBound(framebuffer), "Framebuffer must be bound before attaching a texture");

	if (texture) {
		PTGN_ASSERT(
			gl_.textures.GetCache(texture).size.BothAboveZero(),
			"Cannot attach a texture with no size"
		);
	}

	GLCall(FramebufferTexture2D(
		GL_FRAMEBUFFER, std::to_underlying(attachment),
		std::to_underlying(AttachmentObject::Texture2D), texture, 0
	));

	UpdateFramebufferCache(framebuffer, texture, attachment, AttachmentObject::Texture2D);
}

void Framebuffers::AttachRenderbuffer(
	Framebuffer framebuffer, Renderbuffer renderbuffer, Attachment attachment
) {
	PTGN_ASSERT(
		gl_.IsBound(framebuffer), "Framebuffer must be bound before attaching a renderbuffer"
	);

	if (renderbuffer) {
		PTGN_ASSERT(
			gl_.renderbuffers.GetCache(renderbuffer).size.BothAboveZero(),
			"Cannot attach a renderbuffer with no size"
		);
	}

	GLCall(FramebufferRenderbuffer(
		GL_FRAMEBUFFER, std::to_underlying(attachment),
		std::to_underlying(AttachmentObject::Renderbuffer), renderbuffer
	));

	UpdateFramebufferCache(framebuffer, renderbuffer, attachment, AttachmentObject::Renderbuffer);
}

void Framebuffers::Clear(ClearBufferBit buffers) const {
	GLCall(glClear(std::to_underlying(buffers)));
}

void Framebuffers::ClearToColor(
	Framebuffer framebuffer, Color color, ClearBufferType buffer, int drawbuffer
) const {
	PTGN_ASSERT(gl_.IsBound(framebuffer));
	PTGN_ASSERT(drawbuffer >= 0, "Drawbuffer cannot be negative");
	PTGN_ASSERT(
		buffer == ClearBufferType::Color &&
				static_cast<std::uint32_t>(drawbuffer) < max_color_attachments_ ||
			buffer != ClearBufferType::Color && drawbuffer == 0,
		"Drawbuffer must be 0 for depth and stencil buffers and within max color attachments for "
		"color buffers"
	);
	auto c{ static_cast<V4_float>(color) };
	GLCall(ClearBufferfv(std::to_underlying(buffer), drawbuffer, c.Data()));
}

Framebuffers::PixelValue Framebuffers::ReadPixel(
	Framebuffer framebuffer, V2_int coordinate, Attachment attachment
) {
	auto _1 = gl_.Bind(framebuffer, true);

	auto type		 = GetAttachmentType(attachment);
	const auto& spec = GetFramebufferAttachment(framebuffer, attachment);
	PTGN_ASSERT(spec.id != 0, "No image attached to that attachment");

	V2_int size;
	if (spec.object == AttachmentObject::Texture2D) {
		size = gl_.textures.GetCache(Texture{ spec.id }).size;
	} else {
		size = gl_.renderbuffers.GetCache(Renderbuffer{ spec.id }).size;
	}

	PTGN_ASSERT(
		coordinate.x >= 0 && coordinate.x < size.x,
		"Cannot get pixel out of range of frame buffer size"
	);
	PTGN_ASSERT(
		coordinate.y >= 0 && coordinate.y < size.y,
		"Cannot get pixel out of range of frame buffer size"
	);

	int read_y = size.y - 1 - coordinate.y;

	if (type == AttachmentType::Color) {
		const auto& tex = gl_.textures.GetCache(Texture{ spec.id });

		int components = GetColorComponentCount(tex.internal_format);
		PTGN_ASSERT(components >= 3 && components <= 4);

		std::array<std::uint8_t, 4> v{ 0, 0, 0, 255 };

		GLCall(glReadPixels(
			coordinate.x, read_y, 1, 1, tex.internal_format, GL_UNSIGNED_BYTE, v.data()
		));

		return Color{ v[0], v[1], v[2], components == 4 ? v[3] : static_cast<std::uint8_t>(255) };
	}

	if (type == AttachmentType::Depth) {
		float depth = 0.0f;
		GLCall(glReadPixels(coordinate.x, read_y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth));
		return depth;
	}

	if (type == AttachmentType::Stencil) {
		std::uint8_t stencil = 0;
		GLCall(
			glReadPixels(coordinate.x, read_y, 1, 1, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, &stencil)
		);
		return stencil;
	}

	if (type == AttachmentType::DepthStencil) {
		// GL_DEPTH_STENCIL returns two integers: depth + stencil packed.
		struct {
			std::uint32_t depth;
			std::uint8_t stencil;
		} ds{};

		GLCall(glReadPixels(coordinate.x, read_y, 1, 1, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, &ds)
		);

		float depth = (ds.depth & 0xFFFFFF) / float(0xFFFFFF);
		return std::make_pair(depth, ds.stencil);
	}

	PTGN_ERROR("Unhandled attachment type");
}

Framebuffers::PixelBuffer Framebuffers::ReadPixels(Framebuffer framebuffer, Attachment attachment) {
	auto type = GetAttachmentType(attachment);

	const auto& spec = GetFramebufferAttachment(framebuffer, attachment);
	PTGN_ASSERT(spec.id != 0);

	auto _ = gl_.Bind(framebuffer, true);

	V2_int size = (spec.object == AttachmentObject::Texture2D)
					? gl_.textures.GetCache(Texture{ spec.id }).size
					: gl_.renderbuffers.GetCache(Renderbuffer{ spec.id }).size;

	GLenum format	 = GL_RGBA;
	GLenum type_enum = GL_UNSIGNED_BYTE;

	switch (type) {
		using enum AttachmentType;

		case Color: {
			const auto& tex = gl_.textures.GetCache(Texture{ spec.id });
			PTGN_ASSERT(GetColorComponentCount(tex.internal_format) >= 3);
			format	  = tex.internal_format;
			type_enum = GL_UNSIGNED_BYTE;
			break;
		}
		case Depth:
			format	  = GL_DEPTH_COMPONENT;
			type_enum = GL_FLOAT;
			break;
		case Stencil:
			format	  = GL_STENCIL_INDEX;
			type_enum = GL_UNSIGNED_BYTE;
			break;
		case DepthStencil:
			format	  = GL_DEPTH_STENCIL;
			type_enum = GL_UNSIGNED_INT_24_8;
			break;
	}

	// Allocate max possible size (RGBA8 worst case)
	std::vector<std::uint8_t> buffer(size.x * size.y * 4);

	GLCall(glReadPixels(0, 0, size.x, size.y, format, type_enum, buffer.data()));

	return PixelBuffer{ .size = size, .type = type, .data = std::move(buffer) };
}

bool Framebuffers::FramebufferIsComplete(Framebuffer framebuffer) const {
	PTGN_ASSERT(gl_.IsBound(framebuffer), "Cannot check status of framebuffer until it is bound");
	auto status{ GLCallReturn(CheckFramebufferStatus(GL_FRAMEBUFFER)) };
	return status == GL_FRAMEBUFFER_COMPLETE;
}

const char* Framebuffers::GetFramebufferStatus() const {
	auto status{ GLCallReturn(CheckFramebufferStatus(GL_FRAMEBUFFER)) };
	switch (status) {
		case GL_FRAMEBUFFER_COMPLETE:  return "Framebuffer is complete.";
		case GL_FRAMEBUFFER_UNDEFINED: return "Framebuffer is undefined (no framebuffer bound).";
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
			return "Incomplete attachment: One or more framebuffer attachment points are "
				   "incomplete.";
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
			return "Missing attachment: No images are attached to the framebuffer.";
		case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
			return "Incomplete draw buffer: Draw buffer points to a missing attachment.";
		case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
			return "Incomplete read buffer: Read buffer points to a missing attachment.";
		case GL_FRAMEBUFFER_UNSUPPORTED:
			return "Framebuffer unsupported: Format combination not supported by "
				   "implementation.";
		case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
			return "Incomplete multisample: Mismatched sample counts or improper use of "
				   "multisampling.";
		case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
			return "Incomplete layer targets: Layered attachments are not all complete or "
				   "not "
				   "matching.";
		default: PTGN_ERROR("Unknown framebuffer status.");
	}
}

Framebuffers::AttachmentType Framebuffers::GetAttachmentType(Attachment attachment) const {
	if (attachment >= Attachment::Color0 &&
		std::to_underlying(attachment) <
			std::to_underlying(Attachment::Color0) + max_color_attachments_) {
		return AttachmentType::Color;
	}

	if (attachment == Attachment::Depth) {
		return AttachmentType::Depth;
	}

	if (attachment == Attachment::Stencil) {
		return AttachmentType::Stencil;
	}

	if (attachment == Attachment::DepthStencil) {
		return AttachmentType::DepthStencil;
	}

	PTGN_ERROR("Unsupported framebuffer attachment");
}

AttachmentSpec& Framebuffers::GetFramebufferAttachment(
	Framebuffer framebuffer, Attachment attachment
) {
	return const_cast<AttachmentSpec&>(
		std::as_const(*this).GetFramebufferAttachment(framebuffer, attachment)
	);
}

const AttachmentSpec& Framebuffers::GetFramebufferAttachment(
	Framebuffer framebuffer, Attachment attachment
) const {
	using enum Attachment;

	const auto& cache = cache_.Get(framebuffer);

	if (attachment >= Color0 &&
		std::to_underlying(attachment) < std::to_underlying(Color0) + max_color_attachments_) {
		PTGN_ASSERT(
			attachment >= Color0 &&
				std::to_underlying(attachment) < std::to_underlying(Color0) + cache.color.size(),
			"Color attachment out of valid range"
		);
		auto idx{ std::to_underlying(attachment) - std::to_underlying(Color0) };
		return cache.color[idx];
	} else if (attachment == Depth) {
		return cache.depth;
	} else if (attachment == Stencil) {
		return cache.stencil;
	} else if (attachment == DepthStencil) {
		return cache.depth_stencil;
	} else {
		PTGN_ERROR("Unsupported framebuffer attachment enum");
	}
}

void Framebuffers::UpdateFramebufferCache(
	Framebuffer framebuffer, std::uint32_t object_id, Attachment attachment,
	AttachmentObject object_type
) {
	auto& spec{ GetFramebufferAttachment(framebuffer, attachment) };
	spec.id		= object_id;
	spec.object = object_id ? object_type : AttachmentObject::None;
}

void Framebuffers::ResizeFramebuffer(Framebuffer framebuffer, V2_int new_size) {
	const auto& cache = cache_.Get(framebuffer);

	auto resize_attachment = [&](const AttachmentSpec& spec) {
		if (spec.id == 0) {
			return;
		}

		if (spec.object == AttachmentObject::Texture2D) {
			gl_.textures.ResizeTexture(Texture{ spec.id }, new_size);
		} else if (spec.object == AttachmentObject::Renderbuffer) {
			gl_.renderbuffers.ResizeRenderbuffer(Renderbuffer{ spec.id }, new_size);
		} else {
			PTGN_ERROR("Unknown framebuffer attachment type");
		}
	};

	for (const auto& color : cache.color) {
		resize_attachment(color);
	}

	resize_attachment(cache.depth);
	resize_attachment(cache.stencil);
	resize_attachment(cache.depth_stencil);
}

Framebuffer Framebuffers::CreateFramebufferImpl() {
	Framebuffer id{ 0 };
	GLCall(GenFramebuffers(1, &id.value));
	PTGN_ASSERT(id, "Failed to create framebuffer");
	cache_.Add(id, FramebufferCache{});
	return id;
}

void Framebuffers::DestroyFramebuffer(Framebuffer id) {
	if (!id) {
		return;
	}
	GLCall(DeleteFramebuffers(1, &id.value));
	cache_.Remove(id);
}

void Framebuffers::SavePNG(const path& path, Framebuffer framebuffer, Attachment attachment) {
	// Ensure output directory exists
	if (path.has_parent_path()) {
		std::filesystem::create_directories(path.parent_path());
	}

	// Read all pixels from the framebuffer attachment
	PixelBuffer pb = ReadPixels(framebuffer, attachment);

	PTGN_ASSERT(pb.type == AttachmentType::Color, "SavePNG only supports color attachments");

	const V2_int size	   = pb.size;
	constexpr int channels = 4;

	std::vector<std::uint8_t> rgba(static_cast<std::size_t>(size.x) * size.y * channels);

	// Convert PixelBuffer -> tightly packed RGBA8
	ForEachPixel(pb, [&rgba, size](V2_int pos, const PixelValue& px) {
		const Color* c = std::get_if<Color>(&px);
		PTGN_ASSERT(c != nullptr);

		const std::size_t idx = static_cast<std::size_t>(pos.y * size.x + pos.x) * channels;

		rgba[idx + 0] = c->r;
		rgba[idx + 1] = c->g;
		rgba[idx + 2] = c->b;
		rgba[idx + 3] = c->a;
	});

	SDL_Surface* surface = SDL_CreateSurfaceFrom(
		size.x, size.y, SDL_PIXELFORMAT_RGBA32, rgba.data(), size.x * channels
	);

	PTGN_ASSERT(surface != nullptr, SDL_GetError());

	auto saved{ IMG_SavePNG(surface, path.string().c_str()) };

	PTGN_ASSERT(saved, SDL_GetError());

	SDL_DestroySurface(surface);
}

} // namespace ptgn::impl::gl