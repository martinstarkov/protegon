#include "renderer/backend/gl/gl_framebuffer.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "core/assert.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "core/math/vector4.h"
#include "core/util/file.h"
#include "core/util/id_map.h"
#include "renderer/backend/gl/gl.h"
#include "renderer/backend/gl/gl_context.h"
#include "renderer/backend/gl/gl_renderbuffer.h"
#include "renderer/backend/gl/gl_texture.h"
#include "core/graphics/color.h"
#include "renderer/resources/id.h"
#include "renderer/resources/texture_format.h"

namespace ptgn::impl::gl {

static void ReadPixels(
	V2_int coord, V2_int size, PixelDataFormat format, PixelDataType type, void* data
) {
	GLCall(glReadPixels(
		coord.x, coord.y, size.x, size.y, std::to_underlying(format), std::to_underlying(type), data
	));
}

Attachment ColorAttachment(std::size_t i) {
	PTGN_ASSERT(i <= 8, "Color attachment out of range");
	return Attachment(std::to_underlying(Attachment::Color0) + i);
}

Framebuffers::Framebuffers(GLContext& gl) : gl_{ gl } {}

void Framebuffers::Init(std::uint32_t max_color_attachments) {
	max_color_attachments_ = max_color_attachments;
}

FramebufferId Framebuffers::CreateFramebuffer(
	std::optional<TextureId> texture, Attachment texture_attachment,
	std::optional<RenderbufferId> renderbuffer, Attachment renderbuffer_attachment,
	bool restore_bind
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

void Framebuffers::AttachTexture(
	FramebufferId framebuffer, TextureId texture, Attachment attachment
) {
	PTGN_ASSERT(gl_.IsBound(framebuffer), "FramebufferId must be bound before attaching a texture");

	if (texture) {
		PTGN_ASSERT(
			gl_.textures.GetCache(texture).size.BothAboveZero(),
			"Cannot attach a texture with no size"
		);
	}

	constexpr AttachmentObject texture_target{ AttachmentObject::Texture2D };
	constexpr std::int32_t mipmap_level{ 0 };

	GLCall(glFramebufferTexture2D(
		kFrameBufferTarget, std::to_underlying(attachment), std::to_underlying(texture_target),
		texture, mipmap_level
	));

	UpdateFramebufferCache(framebuffer, texture, attachment, texture_target);
}

void Framebuffers::AttachRenderbuffer(
	FramebufferId framebuffer, RenderbufferId renderbuffer, Attachment attachment
) {
	PTGN_ASSERT(
		gl_.IsBound(framebuffer), "FramebufferId must be bound before attaching a renderbuffer"
	);

	if (renderbuffer) {
		PTGN_ASSERT(
			gl_.renderbuffers.GetCache(renderbuffer).size.BothAboveZero(),
			"Cannot attach a renderbuffer with no size"
		);
	}

	constexpr AttachmentObject renderbuffer_target{ AttachmentObject::Renderbuffer };

	GLCall(glFramebufferRenderbuffer(
		kFrameBufferTarget, std::to_underlying(attachment), std::to_underlying(renderbuffer_target),
		renderbuffer
	));

	UpdateFramebufferCache(framebuffer, renderbuffer, attachment, renderbuffer_target);
}

void Framebuffers::Clear(ClearBufferBit buffers) const {
	GLCall(glClear(std::to_underlying(buffers)));
}

void Framebuffers::ClearToColor(
	FramebufferId framebuffer, Color color, ClearBufferType buffer, int drawbuffer
) const {
	PTGN_ASSERT(gl_.IsBound(framebuffer));
	PTGN_ASSERT(drawbuffer >= 0, "Drawbuffer cannot be negative");
	// TODO: Make function for clearing depth and stencil buffers to specific values.
	PTGN_ASSERT(
		buffer == ClearBufferType::Color &&
			static_cast<std::uint32_t>(drawbuffer) < max_color_attachments_,
		// || buffer != ClearBufferType::Color && drawbuffer == 0,
		"Drawbuffer must be 0 for depth and stencil buffers and within max color attachments for "
		"color buffers"
	);
	auto c{ static_cast<V4_float>(color) };
	GLCall(glClearBufferfv(std::to_underlying(buffer), drawbuffer, c.Data()));
}

Framebuffers::PixelValue Framebuffers::ReadPixel(
	FramebufferId framebuffer, V2_int coordinate, Attachment attachment
) {
	auto _1 = gl_.Bind(framebuffer, true);

	auto type = GetAttachmentType(attachment);

	PTGN_ASSERT(
		type != AttachmentType::Depth && type != AttachmentType::Stencil &&
			type != AttachmentType::DepthStencil,
		"Unsupported attachment type for reading pixels"
	);

	const auto& spec = GetFramebufferAttachment(framebuffer, attachment);
	PTGN_ASSERT(spec.id != 0, "No image attached to that attachment");

	V2_int size;
	TextureFormat format{};

	if (spec.object == AttachmentObject::Texture2D) {
		const auto& cache{ gl_.textures.GetCache(TextureId{ spec.id }) };
		size   = cache.size;
		format = cache.format;
	} else {
		const auto& cache{ gl_.renderbuffers.GetCache(RenderbufferId{ spec.id }) };
		size   = cache.size;
		format = cache.format;
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

	constexpr V2_int query_size{ 1, 1 };

	V2_int coord{ coordinate.x, read_y };

	if (type == AttachmentType::Color) {
		const auto& tex = gl_.textures.GetCache(TextureId{ spec.id });

		int components = GetBitCount(tex.format);
		PTGN_ASSERT(components >= 3 && components <= 4);

		std::array<std::uint8_t, 4> v{ 0, 0, 0, 255 };

		auto [pixel_format, pixel_type] = GetPixelDataFormat(format);

		ptgn::impl::gl::ReadPixels(coord, query_size, pixel_format, pixel_type, v.data());

		return Color{ v[0], v[1], v[2], components == 4 ? v[3] : static_cast<std::uint8_t>(255) };
	}
	/*
	if (type == AttachmentType::Depth) {
		float depth						= 0.0f;
		auto [pixel_format, pixel_type] = GetPixelDataFormat(format);
		// TODO: Fix to use the correct pixel format and type.
		ptgn::impl::gl::ReadPixels(coord, query_size, PixelDataFormat::Depth, PixelDataType::Float,
	&depth); , return depth;
	}
	if (type == AttachmentType::Stencil) {
		std::uint8_t stencil = 0;
		auto [pixel_format, pixel_type] = GetPixelDataFormat(format);
		// TODO: Fix to use the correct pixel format and type.
		ptgn::impl::gl::ReadPixels(coord, query_size, PixelDataFormat::Stencil,
	PixelDataType::UnsignedByte, &stencil); return stencil;
	}
	if (type == AttachmentType::DepthStencil) {
		// PixelDataFormat::DepthStencil returns two integers: depth + stencil packed.
		struct {
			std::uint32_t depth;
			std::uint8_t stencil;
		} ds{};
		auto [pixel_format, pixel_type] = GetPixelDataFormat(format);
		// TODO: Fix to use the correct pixel format and type.
		ptgn::impl::gl::ReadPixels(coord, query_size, PixelDataFormat::DepthStencil,
	PixelDataType::UnsignedInt_24_8, &ds); float depth = (ds.depth & 0xFFFFFF) / float(0xFFFFFF);
		return std::make_pair(depth, ds.stencil);
	}
	*/
	PTGN_ERROR("Unhandled attachment type");
}

std::vector<AttachmentSpec> Framebuffers::GetAttachments(FramebufferId framebuffer) const {
	std::vector<AttachmentSpec> attachments;

	if (!framebuffer) {
		return attachments;
	}

	PTGN_ASSERT(cache_.Has(framebuffer), "No framebuffer with id ", framebuffer, " in cache");

	const auto& cache = cache_.Get(framebuffer);

	for (const auto& color_attachment : cache.color) {
		if (color_attachment.id != 0) {
			attachments.push_back(color_attachment);
		}
	}

	if (cache.depth.id != 0) {
		attachments.push_back(cache.depth);
	}

	if (cache.stencil.id != 0) {
		attachments.push_back(cache.stencil);
	}

	if (cache.depth_stencil.id != 0) {
		attachments.push_back(cache.depth_stencil);
	}

	return attachments;
}

Framebuffers::PixelBuffer Framebuffers::ReadPixels(
	FramebufferId framebuffer, Attachment attachment
) {
	auto type = GetAttachmentType(attachment);

	PTGN_ASSERT(
		type != AttachmentType::Depth && type != AttachmentType::Stencil &&
			type != AttachmentType::DepthStencil,
		"Unsupported attachment type for reading pixels"
	);

	const auto& spec = GetFramebufferAttachment(framebuffer, attachment);
	PTGN_ASSERT(spec.id != 0);

	auto _ = gl_.Bind(framebuffer, true);

	V2_int size = (spec.object == AttachmentObject::Texture2D)
					? gl_.textures.GetCache(TextureId{ spec.id }).size
					: gl_.renderbuffers.GetCache(RenderbufferId{ spec.id }).size;

	TextureFormat format = (spec.object == AttachmentObject::Texture2D)
							 ? gl_.textures.GetCache(TextureId{ spec.id }).format
							 : gl_.renderbuffers.GetCache(RenderbufferId{ spec.id }).format;

	auto [pixel_format, pixel_type] = GetPixelDataFormat(format);

	// TODO: Fix to use the correct pixel format and type for the buffer.
	// Allocate max possible size (RGBA8 worst case)
	std::vector<std::uint8_t> buffer(size.x * size.y * 4);

	ptgn::impl::gl::ReadPixels({ 0, 0 }, size, pixel_format, pixel_type, buffer.data());

	return PixelBuffer{ .size = size, .type = type, .data = std::move(buffer) };
}

bool Framebuffers::FramebufferIsComplete(FramebufferId framebuffer) const {
	PTGN_ASSERT(gl_.IsBound(framebuffer), "Cannot check status of framebuffer until it is bound");
	auto status{ GLCallReturn(glCheckFramebufferStatus(kFrameBufferTarget)) };
	return status == GL_FRAMEBUFFER_COMPLETE;
}

const char* Framebuffers::GetFramebufferStatus() const {
	auto status{ GLCallReturn(glCheckFramebufferStatus(kFrameBufferTarget)) };
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
	FramebufferId framebuffer, Attachment attachment
) {
	return const_cast<AttachmentSpec&>( // NOSONAR
		std::as_const(*this).GetFramebufferAttachment(framebuffer, attachment)
	);
}

const AttachmentSpec& Framebuffers::GetFramebufferAttachment(
	FramebufferId framebuffer, Attachment attachment
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
	FramebufferId framebuffer, std::uint32_t object_id, Attachment attachment,
	AttachmentObject object_type
) {
	auto& spec{ GetFramebufferAttachment(framebuffer, attachment) };
	spec.id		= object_id;
	spec.object = object_id ? object_type : AttachmentObject::None;
}

void Framebuffers::ResizeFramebuffer(FramebufferId framebuffer, V2_int new_size) {
	const auto& cache = cache_.Get(framebuffer);

	auto resize_attachment = [&](const AttachmentSpec& spec) {
		if (spec.id == 0) {
			return;
		}

		if (spec.object == AttachmentObject::Texture2D) {
			gl_.textures.ResizeTexture(TextureId{ spec.id }, new_size);
		} else if (spec.object == AttachmentObject::Renderbuffer) {
			gl_.renderbuffers.ResizeRenderbuffer(RenderbufferId{ spec.id }, new_size);
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

FramebufferId Framebuffers::CreateFramebufferImpl() {
	FramebufferId id{ 0 };
	GLCall(glGenFramebuffers(1, &id.value));
	PTGN_ASSERT(id, "Failed to create framebuffer");
	cache_.Add(id, FramebufferCache{});
	return id;
}

Framebuffers::PixelValue Framebuffers::DecodePixel(
	const std::vector<std::uint8_t>& data, int idx, AttachmentType type
) {
	// TODO: Fix this function to work with format.
	switch (type) {
		case AttachmentType::Color: {
			const std::uint8_t* p = &data[idx * 4];
			return Color{ p[0], p[1], p[2], p[3] };
		}

		case AttachmentType::Depth: {
			const float* p = reinterpret_cast<const float*>(data.data());
			return p[idx];
		}

		case AttachmentType::Stencil: {
			return data[idx];
		}

		case AttachmentType::DepthStencil: {
			const auto* p = reinterpret_cast<const std::uint32_t*>(data.data());

			const std::uint32_t packed = p[idx];

			float depth = float(packed & 0xFFFFFF) / float(0xFFFFFF);

			std::uint8_t stencil = (packed >> 24) & 0xFF;

			return std::make_pair(depth, stencil);
		}
	}

	PTGN_ERROR("Unknown AttachmentType");
}

template <typename IdT, typename AttachFn>
void InvalidateAttachment(
	GLContext& gl, IdMap<FramebufferCache>& cache, IdT resource, AttachmentObject type,
	AttachFn attach
) {
	// Iterate through all framebuffers and detach the given resource from any attachments it is
	// currently attached to.

	for (auto item : cache.Items()) {
		FramebufferId fbo{ static_cast<std::uint32_t>(item.id) };

		constexpr std::size_t kMaxAttachments{ 8ULL + 3ULL };

		std::array<Attachment, kMaxAttachments> pending{};
		std::size_t count = 0;

		for (std::size_t i = 0; i < item.value.color.size(); ++i) {
			auto& a = item.value.color[i];

			if (a.id == resource && a.object == type) {
				a = {};
				PTGN_ASSERT(count < kMaxAttachments);
				pending[count] = ColorAttachment(i);
				count++;
			}
		}

		if (item.value.depth.id == resource && item.value.depth.object == type) {
			item.value.depth = {};
			PTGN_ASSERT(count < kMaxAttachments);
			pending[count] = Attachment::Depth;
			count++;
		}

		if (item.value.depth_stencil.id == resource && item.value.depth_stencil.object == type) {
			item.value.depth_stencil = {};
			PTGN_ASSERT(count < kMaxAttachments);
			pending[count] = Attachment::DepthStencil;
			count++;
		}

		if (item.value.stencil.id == resource && item.value.stencil.object == type) {
			item.value.stencil = {};
			PTGN_ASSERT(count < kMaxAttachments);
			pending[count] = Attachment::Stencil;
			count++;
		}

		if (count == 0) {
			continue;
		}

		auto _ = gl.Bind(fbo, true);

		for (Attachment attachment : pending) {
			std::invoke(attach, fbo, IdT{ 0 }, attachment);
		}
	}
}

void Framebuffers::InvalidateTexture(TextureId texture) {
	InvalidateAttachment(
		gl_, cache_, texture, AttachmentObject::Texture2D,
		[this](FramebufferId fbo, TextureId tex, Attachment a) { AttachTexture(fbo, tex, a); }
	);
}

void Framebuffers::InvalidateRenderbuffer(RenderbufferId renderbuffer) {
	InvalidateAttachment(
		gl_, cache_, renderbuffer, AttachmentObject::Renderbuffer,
		[this](FramebufferId fbo, RenderbufferId r, Attachment a) { AttachRenderbuffer(fbo, r, a); }
	);
}

void Framebuffers::DestroyFramebufferOwning(FramebufferId id) {
	auto attachments{ GetAttachments(id) };

	for (const auto& attachment : attachments) {
		PTGN_ASSERT(attachment.id != 0);

		if (attachment.object == AttachmentObject::Texture2D) {
			gl_.textures.DestroyTexture(TextureId{ attachment.id });
		} else if (attachment.object == AttachmentObject::Renderbuffer) {
			gl_.renderbuffers.DestroyRenderbuffer(RenderbufferId{ attachment.id });
		}
	}

	DestroyFramebuffer(id);
}

void Framebuffers::DestroyFramebuffer(FramebufferId id) {
	if (!id) {
		return;
	}
	GLCall(glDeleteFramebuffers(1, &id.value));
	cache_.Remove(id);
}

void Framebuffers::SavePNG(const path& path, FramebufferId framebuffer, Attachment attachment) {
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

	// TODO: Fix.
	/*
	SDL_Surface* surface = SDL_CreateSurfaceFrom(
		size.x, size.y, SDL_PIXELFORMAT_RGBA32, rgba.data(), size.x * channels
	);

	PTGN_ASSERT(surface != nullptr, SDL_GetError());

	auto saved{ IMG_SavePNG(surface, path.string().c_str()) };

	PTGN_ASSERT(saved, SDL_GetError());

	SDL_DestroySurface(surface);
	*/
}

std::ostream& operator<<(std::ostream& os, AttachmentObject object) {
	switch (object) {
		using enum AttachmentObject;
		case None:		   return os << "None";
		case Texture2D:	   return os << "Texture2D";
		case Renderbuffer: return os << "Renderbuffer";
		default:		   PTGN_ERROR("Unknown AttachmentObject: ", std::to_underlying(object));
	}
}

std::ostream& operator<<(std::ostream& os, ClearBufferBit bits) {
	if (bits == ClearBufferBit::None) {
		return os << "None";
	}

	bool first = true;

	auto print_flag = [&](ClearBufferBit flag, const char* name) {
		if ((bits & flag) == flag) {
			if (!first) {
				os << " | ";
			}
			os << name;
			first = false;
		}
	};

	print_flag(ClearBufferBit::Color, "Color");
	print_flag(ClearBufferBit::Depth, "Depth");
	print_flag(ClearBufferBit::Stencil, "Stencil");

	return os;
}

std::ostream& operator<<(std::ostream& os, ClearBufferType clear_buffer_type) {
	switch (clear_buffer_type) {
		using enum ClearBufferType;
		case Color:	  return os << "Color";
		case Depth:	  return os << "Depth";
		case Stencil: return os << "Stencil";
		default:	  PTGN_ERROR("Unknown ClearBufferType: ", std::to_underlying(clear_buffer_type));
	}
}

std::ostream& operator<<(std::ostream& os, Attachment attachment) {
	switch (attachment) {
		using enum Attachment;
		case Color0:	   return os << "Color0";
		case Color1:	   return os << "Color1";
		case Color2:	   return os << "Color2";
		case Color3:	   return os << "Color3";
		case Color4:	   return os << "Color4";
		case Color5:	   return os << "Color5";
		case Color6:	   return os << "Color6";
		case Color7:	   return os << "Color7";
		case Color8:	   return os << "Color8";
		case Depth:		   return os << "Depth";
		case Stencil:	   return os << "Stencil";
		case DepthStencil: return os << "DepthStencil";
		default:		   PTGN_ERROR("Unknown Attachment: ", std::to_underlying(attachment));
	}
}

} // namespace ptgn::impl::gl