#include "renderer/frame_buffer.h"

#include <cstdint>
#include <utility>

#include "core/game.h"
#include "math/vector2.h"
#include "renderer/gl_helper.h"
#include "renderer/gl_loader.h"
#include "renderer/gl_renderer.h"
#include "renderer/renderer.h"
#include "renderer/texture.h"
#include "utility/assert.h"
#include "utility/assert.h"
#include "utility/handle.h"
#include "utility/stats.h"

namespace ptgn::impl {

RenderBuffer::RenderBuffer(const V2_int& size) {
	GenerateRenderBuffer();
	Bind();
	GLCall(gl::RenderbufferStorage(
		GL_RENDERBUFFER, static_cast<gl::GLenum>(InternalGLDepthFormat::DEPTH24_STENCIL8), size.x,
		size.y
	));
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
	GLCall(gl::GenRenderbuffers(1, &id_));
	PTGN_ASSERT(IsValid(), "Failed to generate render buffer using OpenGL context");
#ifdef GL_ANNOUNCE_RENDER_BUFFER_CALLS
	PTGN_LOG("GL: Generated render buffer with id ", id_);
#endif
}

void RenderBuffer::DeleteRenderBuffer() noexcept {
	if (!IsValid()) {
		return;
	}
	GLCall(gl::DeleteRenderbuffers(1, &id_));
#ifdef GL_ANNOUNCE_RENDER_BUFFER_CALLS
	PTGN_LOG("GL: Deleted render buffer with id ", id_);
#endif
}

void RenderBuffer::Bind(std::uint32_t id) {
	GLCall(gl::BindRenderbuffer(GL_RENDERBUFFER, id));
#ifdef GL_ANNOUNCE_RENDER_BUFFER_CALLS
	PTGN_LOG("GL: Bound render buffer with id ", id);
#endif
}

void RenderBuffer::Bind() const {
	Bind(id_);
}

void RenderBuffer::Unbind() {
	Bind(0);
}

std::uint32_t RenderBuffer::GetBoundId() {
	std::int32_t id{ -1 };
	GLCall(gl::glGetIntegerv(static_cast<gl::GLenum>(impl::GLBinding::RenderBuffer), &id));
	PTGN_ASSERT(id >= 0, "Failed to retrieve bound render buffer id");
	return static_cast<std::uint32_t>(id);
}

std::uint32_t RenderBuffer::GetId() const {
	return id_;
}

bool RenderBuffer::IsValid() const {
	return id_;
}

FrameBuffer::FrameBuffer(Texture&& texture) {
	GenerateFrameBuffer();
	Bind();
	AttachTexture(std::move(texture));
}

FrameBuffer::FrameBuffer(RenderBuffer&& render_buffer) {
	GenerateFrameBuffer();
	Bind();
	AttachRenderBuffer(std::move(render_buffer));
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
	GLCall(gl::GenFramebuffers(1, &id_));
	PTGN_ASSERT(IsValid(), "Failed to generate frame buffer using OpenGL context");
#ifdef GL_ANNOUNCE_FRAME_BUFFER_CALLS
	PTGN_LOG("GL: Generated frame buffer with id ", id_);
#endif
}

void FrameBuffer::DeleteFrameBuffer() noexcept {
	if (!IsValid()) {
		return;
	}
	GLCall(gl::DeleteFramebuffers(1, &id_));
#ifdef GL_ANNOUNCE_FRAME_BUFFER_CALLS
	PTGN_LOG("GL: Deleted frame buffer with id ", id_);
#endif
}

void FrameBuffer::AttachTexture(Texture&& texture) {
	PTGN_ASSERT(texture.IsValid(), "Cannot attach invalid texture to frame buffer");
	PTGN_ASSERT(IsBound(), "Cannot attach texture until frame buffer is bound");
	texture_ = std::move(texture);
	GLCall(gl::FramebufferTexture2D(
		GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_.GetId(), 0
	));
	PTGN_ASSERT(IsComplete(), "Failed to attach texture to frame buffer");
}

void FrameBuffer::AttachRenderBuffer(RenderBuffer&& render_buffer) {
	PTGN_ASSERT(render_buffer.IsValid(), "Cannot attach invalid render buffer to frame buffer");
	PTGN_ASSERT(IsBound(), "Cannot attach render buffer until frame buffer is bound");
	render_buffer_ = std::move(render_buffer);
	GLCall(gl::FramebufferRenderbuffer(
		GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, render_buffer_.GetId()
	));
	PTGN_ASSERT(IsComplete(), "Failed to attach render buffer to frame buffer");
}

bool FrameBuffer::IsBound() const {
	return GetBoundId() == id_;
}

bool FrameBuffer::IsComplete() const {
	PTGN_ASSERT(IsBound(), "Cannot check status of frame buffer until it is bound");
	auto status{ GLCallReturn(gl::CheckFramebufferStatus(GL_FRAMEBUFFER)) };
	return status == GL_FRAMEBUFFER_COMPLETE;
	// TODO: Consider adding a way to query frame buffer status.
	// PTGN_ERROR("Incomplete FrameBuffer: ", status);
}

const Texture& FrameBuffer::GetTexture() const {
	return texture_;
}

const RenderBuffer& FrameBuffer::GetRenderBuffer() const {
	return render_buffer_;
}

void FrameBuffer::Bind(std::uint32_t id) {
	if (game.renderer.bound_.frame_buffer_id == id) {
		return;
	}
	GLCall(gl::BindFramebuffer(GL_FRAMEBUFFER, id));
	game.renderer.bound_.frame_buffer_id = id;
#ifdef PTGN_DEBUG
	++game.stats.frame_buffer_binds;
#endif
#ifdef GL_ANNOUNCE_FRAME_BUFFER_CALLS
	PTGN_LOG("GL: Bound frame buffer with id ", id);
#endif
}

bool FrameBuffer::IsUnbound() {
	return GetBoundId() == 0;
}

bool FrameBuffer::IsValid() const {
	return id_;
}

void FrameBuffer::Bind() const {
	Bind(id_);
}

void FrameBuffer::Unbind() {
	Bind(0);
}

std::uint32_t FrameBuffer::GetBoundId() {
	std::int32_t id{ -1 };
	GLCall(gl::glGetIntegerv(static_cast<gl::GLenum>(impl::GLBinding::FrameBufferDraw), &id));
	PTGN_ASSERT(id >= 0, "Failed to retrieve bound frame buffer id");
	return static_cast<std::uint32_t>(id);
}

} // namespace ptgn::impl