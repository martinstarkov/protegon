#include "renderer/buffers/frame_buffer.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include "core/app/game.h"
#include "debug/core/debug_config.h"
#include "debug/runtime/assert.h"
#include "debug/runtime/debug_system.h"
#include "debug/runtime/stats.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "renderer/gl/gl_helper.h"
#include "renderer/gl/gl_loader.h"
#include "renderer/gl/gl_renderer.h"
#include "renderer/gl/gl_types.h"
#include "renderer/materials/texture.h"
#include "renderer/renderer.h"

namespace ptgn::impl {

RenderBuffer::RenderBuffer(const V2_int& size, InternalGLFormat format) {
	GenerateRenderBuffer();
	RenderBufferId restore_render_buffer_id{ RenderBuffer::GetBoundId() };
	Bind();
	SetStorage(size, format);
	RenderBuffer::BindId(restore_render_buffer_id);
}

void RenderBuffer::SetStorage(const V2_int& size, InternalGLFormat format) {
	PTGN_ASSERT(IsBound(), "Render buffer must be bound prior to setting its storage");

	GLCall(RenderbufferStorage(GL_RENDERBUFFER, static_cast<GLenum>(format), size.x, size.y));

	size_	= size;
	format_ = format;
}

RenderBuffer::RenderBuffer(RenderBuffer&& other) noexcept : id_{ std::exchange(other.id_, 0) } {}

RenderBuffer& RenderBuffer::operator=(RenderBuffer&& other) noexcept {
	if (this != &other) {
		DeleteRenderBuffer();
		id_ = std::exchange(other.id_, 0);
	}
	return *this;
}

RenderBuffer::~RenderBuffer() {
	DeleteRenderBuffer();
}

void RenderBuffer::GenerateRenderBuffer() {
	GLCall(GenRenderbuffers(1, &id_));
	PTGN_ASSERT(IsValid(), "Failed to generate render buffer using OpenGL context");
#ifdef GL_ANNOUNCE_RENDER_BUFFER_CALLS
	PTGN_LOG("GL: Generated render buffer with id ", id_);
#endif
}

void RenderBuffer::DeleteRenderBuffer() noexcept {
	if (!IsValid()) {
		return;
	}
	GLCall(DeleteRenderbuffers(1, &id_));
#ifdef GL_ANNOUNCE_RENDER_BUFFER_CALLS
	PTGN_LOG("GL: Deleted render buffer with id ", id_);
#endif
	id_ = 0;
}

void RenderBuffer::BindId(RenderBufferId id) {
	if (game.renderer.bound_.render_buffer_id == id) {
		return;
	}
	game.renderer.bound_.render_buffer_id = id;
	GLCall(BindRenderbuffer(GL_RENDERBUFFER, id));
#ifdef PTGN_DEBUG
	++game.debug.stats.render_buffer_binds;
#endif
#ifdef GL_ANNOUNCE_RENDER_BUFFER_CALLS
	PTGN_LOG("GL: Bound render buffer with id ", id);
#endif
}

bool RenderBuffer::IsBound() const {
	auto bound_id{ GetBoundId() };
	return bound_id == id_;
}

void RenderBuffer::Bind() const {
	PTGN_ASSERT(IsValid(), "Cannot bind destroyed or uninitialized render buffer");
	BindId(id_);
}

void RenderBuffer::Unbind() {
	BindId(0);
}

RenderBufferId RenderBuffer::GetBoundId() {
	std::int32_t id{ -1 };
	GLCall(glGetIntegerv(static_cast<GLenum>(impl::GLBinding::RenderBuffer), &id));
	PTGN_ASSERT(id >= 0, "Failed to retrieve bound render buffer id");
	return static_cast<RenderBufferId>(id);
}

bool RenderBuffer::IsValid() const {
	return id_;
}

RenderBufferId RenderBuffer::GetId() const {
	return id_;
}

InternalGLFormat RenderBuffer::GetFormat() const {
	return format_;
}

V2_int RenderBuffer::GetSize() const {
	return size_;
}

void RenderBuffer::Resize(const V2_int& new_size) {
	if (!IsValid() || size_ == new_size) {
		return;
	}

	RenderBufferId restore_render_buffer_id{ RenderBuffer::GetBoundId() };
	Bind();
	SetStorage(new_size, GetFormat());
	RenderBuffer::BindId(restore_render_buffer_id);
}

FrameBuffer::FrameBuffer(Texture&& texture, bool bind_frame_buffer) {
	GenerateFrameBuffer();
	std::optional<FrameBufferId> restore_frame_buffer_id;
	if (!bind_frame_buffer) {
		restore_frame_buffer_id = FrameBuffer::GetBoundId();
	}
	Bind();
	V2_int size{ texture.GetSize() };
	PTGN_ASSERT(
		texture.GetFormat() != TextureFormat::Depth24 &&
		texture.GetFormat() != TextureFormat::Depth24_Stencil8
	);
	AttachTexture(std::move(texture), FrameBufferAttachment::Color0);
	RenderBuffer rbo{ size, InternalGLFormat::DEPTH24_STENCIL8 };
	AttachRenderBuffer(std::move(rbo), FrameBufferAttachment::DepthStencil);
	if (restore_frame_buffer_id.has_value()) {
		FrameBuffer::BindId(*restore_frame_buffer_id);
	}
}

FrameBuffer::FrameBuffer(FrameBuffer&& other) noexcept :
	id_{ std::exchange(other.id_, 0) },
	texture_{ std::exchange(other.texture_, {}) },
	render_buffer_{ std::exchange(other.render_buffer_, {}) } {}

FrameBuffer& FrameBuffer::operator=(FrameBuffer&& other) noexcept {
	if (this != &other) {
		DeleteFrameBuffer();
		id_			   = std::exchange(other.id_, 0);
		texture_	   = std::exchange(other.texture_, {});
		render_buffer_ = std::exchange(other.render_buffer_, {});
	}
	return *this;
}

FrameBuffer::~FrameBuffer() {
	DeleteFrameBuffer();
}

void FrameBuffer::GenerateFrameBuffer() {
	GLCall(GenFramebuffers(1, &id_));
	PTGN_ASSERT(IsValid(), "Failed to generate frame buffer using OpenGL context");
#ifdef GL_ANNOUNCE_FRAME_BUFFER_CALLS
	PTGN_LOG("GL: Generated frame buffer with id ", id_);
#endif
}

void FrameBuffer::DeleteFrameBuffer() noexcept {
	if (!IsValid()) {
		return;
	}
	GLCall(DeleteFramebuffers(1, &id_));
#ifdef GL_ANNOUNCE_FRAME_BUFFER_CALLS
	PTGN_LOG("GL: Deleted frame buffer with id ", id_);
#endif
	id_ = 0;
}

void FrameBuffer::SetDrawBuffer(FrameBufferAttachment attachment) {
	if (attachment != FrameBufferAttachment::DepthStencil &&
		attachment != FrameBufferAttachment::Stencil &&
		attachment != FrameBufferAttachment::Depth) {
		std::vector<GLenum> attachments{ static_cast<GLenum>(attachment) };
		GLCall(DrawBuffers(static_cast<GLsizei>(attachments.size()), attachments.data()));
	} else {
		glDrawBuffer(GL_NONE);
	}
}

void FrameBuffer::AttachTexture(Texture&& texture, FrameBufferAttachment attachment) {
	PTGN_ASSERT(texture.IsValid(), "Cannot attach invalid texture to frame buffer");
	PTGN_ASSERT(IsBound(), "Cannot attach texture until frame buffer is bound");
	PTGN_ASSERT(
		texture.GetSize().BothAboveZero(), "Cannot attach texture with no size to a frame buffer"
	);
	texture_ = std::move(texture);
	GLCall(FramebufferTexture2D(
		GL_FRAMEBUFFER, static_cast<GLenum>(attachment), GL_TEXTURE_2D, texture_.GetId(), 0
	));
	PTGN_ASSERT(IsComplete(), "Failed to attach texture to frame buffer: ", GetStatus());
}

void FrameBuffer::AttachRenderBuffer(
	RenderBuffer&& render_buffer, FrameBufferAttachment attachment
) {
	PTGN_ASSERT(render_buffer.IsValid(), "Cannot attach invalid render buffer to frame buffer");
	PTGN_ASSERT(IsBound(), "Cannot attach render buffer until frame buffer is bound");
	render_buffer_ = std::move(render_buffer);
	GLCall(FramebufferRenderbuffer(
		GL_FRAMEBUFFER, static_cast<GLenum>(attachment), GL_RENDERBUFFER, render_buffer_.GetId()
	));
	PTGN_ASSERT(IsComplete(), "Failed to attach render buffer to frame buffer: ", GetStatus());
}

bool FrameBuffer::IsComplete() const {
	PTGN_ASSERT(IsBound(), "Cannot check status of frame buffer until it is bound");
	auto status{ GLCallReturn(CheckFramebufferStatus(GL_FRAMEBUFFER)) };
	return status == GL_FRAMEBUFFER_COMPLETE;
}

const char* FrameBuffer::GetStatus() const {
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
			return "Framebuffer unsupported: Format combination not supported by implementation.";
		case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
			return "Incomplete multisample: Mismatched sample counts or improper use of "
				   "multisampling.";
		case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
			return "Incomplete layer targets: Layered attachments are not all complete or not "
				   "matching.";
		default: return "Unknown framebuffer status.";
	}
}

void FrameBuffer::Resize(const V2_int& size) {
	texture_.Resize(size);
	render_buffer_.Resize(size);
}

const Texture& FrameBuffer::GetTexture() const {
	return texture_;
}

Texture& FrameBuffer::GetTexture() {
	return texture_;
}

const RenderBuffer& FrameBuffer::GetRenderBuffer() const {
	return render_buffer_;
}

void FrameBuffer::BindId(FrameBufferId id) {
	if (game.renderer.bound_.frame_buffer_id == id) {
		return;
	}
	GLCall(BindFramebuffer(GL_FRAMEBUFFER, id));
	game.renderer.bound_.frame_buffer_id = id;
#ifdef PTGN_DEBUG
	++game.debug.stats.frame_buffer_binds;
#endif
#ifdef GL_ANNOUNCE_FRAME_BUFFER_CALLS
	PTGN_LOG("GL: Bound frame buffer with id ", id);
#endif
}

bool FrameBuffer::IsBound() const {
	auto bound_id{ GetBoundId() };
	return bound_id == id_;
}

bool FrameBuffer::IsUnbound() {
	auto bound_id{ GetBoundId() };
	return bound_id == 0;
}

void FrameBuffer::ClearToColor(const Color& color) const {
	Bind();
	PTGN_ASSERT(IsBound(), "Frame buffer must be bound before clearing");
	impl::GLRenderer::ClearToColor(color);
}

bool FrameBuffer::IsValid() const {
	return id_;
}

FrameBufferId FrameBuffer::GetId() const {
	return id_;
}

void FrameBuffer::Bind() const {
	BindId(id_);
}

void FrameBuffer::Unbind() {
	BindId(0);
}

FrameBufferId FrameBuffer::GetBoundId() {
	std::int32_t id{ -1 };
	GLCall(glGetIntegerv(static_cast<GLenum>(impl::GLBinding::FrameBufferDraw), &id));
	PTGN_ASSERT(id >= 0, "Failed to retrieve bound frame buffer id");
	return static_cast<FrameBufferId>(id);
}

Color FrameBuffer::GetPixel(const V2_int& coordinate, bool restore_bind_state) const {
	// TODO: Allow reading pixels from stencil or depth buffers.

	V2_int size{ texture_.GetSize() };
	PTGN_ASSERT(
		coordinate.x >= 0 && coordinate.x < size.x,
		"Cannot get pixel out of range of frame buffer texture"
	);
	PTGN_ASSERT(
		coordinate.y >= 0 && coordinate.y < size.y,
		"Cannot get pixel out of range of frame buffer texture"
	);
	std::optional<TextureId> restore_texture_id;
	std::optional<FrameBufferId> restore_frame_buffer_id;
	if (restore_bind_state) {
		restore_texture_id		= Texture::GetBoundId();
		restore_frame_buffer_id = FrameBuffer::GetBoundId();
	}
	texture_.Bind();
	auto formats{ impl::GetGLFormats(texture_.GetFormat()) };
	PTGN_ASSERT(
		formats.color_components >= 3,
		"Textures with less than 3 pixel components cannot currently be queried"
	);
	std::vector<std::uint8_t> v(static_cast<std::size_t>(formats.color_components * 1 * 1));
	int y{ size.y - 1 - coordinate.y };
	PTGN_ASSERT(y >= 0);
	Bind();
	GLCall(glReadPixels(
		coordinate.x, y, 1, 1, static_cast<GLenum>(formats.input_format),
		static_cast<GLenum>(impl::GLType::UnsignedByte), static_cast<void*>(v.data())
	));
	if (restore_texture_id.has_value()) {
		Texture::BindId(*restore_texture_id);
	}
	if (restore_frame_buffer_id.has_value()) {
		FrameBuffer::BindId(*restore_frame_buffer_id);
	}
	return Color{ v[0], v[1], v[2],
				  formats.color_components == 4 ? v[3] : static_cast<std::uint8_t>(255) };
}

void FrameBuffer::ForEachPixel(
	const std::function<void(V2_int, Color)>& func, bool restore_bind_state
) const {
	// TODO: Allow reading pixels from stencil or depth buffers.

	V2_int size{ texture_.GetSize() };

	std::optional<TextureId> restore_texture_id;
	std::optional<FrameBufferId> restore_frame_buffer_id;
	if (restore_bind_state) {
		restore_texture_id		= Texture::GetBoundId();
		restore_frame_buffer_id = FrameBuffer::GetBoundId();
	}

	texture_.Bind();
	auto formats{ impl::GetGLFormats(texture_.GetFormat()) };
	PTGN_ASSERT(
		formats.color_components >= 3,
		"Textures with less than 3 pixel components cannot currently be queried"
	);

	std::vector<std::uint8_t> v(static_cast<std::size_t>(formats.color_components * size.x * size.y)
	);
	Bind();
	GLCall(glReadPixels(
		0, 0, size.x, size.y, static_cast<GLenum>(formats.input_format),
		static_cast<GLenum>(impl::GLType::UnsignedByte), static_cast<void*>(v.data())
	));
	for (int j{ 0 }; j < size.y; j++) {
		// Ensure left-to-right and top-to-bottom iteration.
		int row{ (size.y - 1 - j) * size.x * formats.color_components };
		for (int i{ 0 }; i < size.x; i++) {
			int idx{ row + i * formats.color_components };
			PTGN_ASSERT(static_cast<std::size_t>(idx) < v.size());
			Color color{ v[static_cast<std::size_t>(idx)], v[static_cast<std::size_t>(idx + 1)],
						 v[static_cast<std::size_t>(idx + 2)],
						 formats.color_components == 4 ? v[static_cast<std::size_t>(idx + 3)]
													   : static_cast<std::uint8_t>(255) };
			func(V2_int{ i, j }, color);
		}
	}
	if (restore_texture_id.has_value()) {
		Texture::BindId(*restore_texture_id);
	}
	if (restore_frame_buffer_id.has_value()) {
		FrameBuffer::BindId(*restore_frame_buffer_id);
	}
}

} // namespace ptgn::impl