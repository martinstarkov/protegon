#include "renderer/frame_buffer.h"

#include <cstdint>

#include "core/game.h"
#include "math/vector2.h"
#include "renderer/gl_helper.h"
#include "renderer/gl_loader.h"
#include "renderer/gl_renderer.h"
#include "renderer/renderer.h"
#include "renderer/texture.h"
#include "utility/debug.h"
#include "utility/handle.h"
#include "utility/stats.h"

namespace ptgn {

namespace impl {

FrameBufferInstance::FrameBufferInstance() {
	GLCall(gl::GenFramebuffers(1, &id_));
	PTGN_ASSERT(id_ != 0, "Failed to generate frame buffer using OpenGL context");
#ifdef GL_ANNOUNCE_FRAME_BUFFER_CALLS
	PTGN_LOG("GL: Generated frame buffer with id ", id_);
#endif
}

FrameBufferInstance::~FrameBufferInstance() {
	GLCall(gl::DeleteFramebuffers(1, &id_));
#ifdef GL_ANNOUNCE_FRAME_BUFFER_CALLS
	PTGN_LOG("GL: Deleted frame buffer with id ", id_);
#endif
}

void FrameBufferInstance::AttachTexture(const Texture& texture) {
	PTGN_ASSERT(texture.IsValid(), "Cannot attach invalid texture to frame buffer");
	PTGN_ASSERT(IsBound(), "Cannot attach texture until frame buffer is bound");
	texture_ = texture;
	GLCall(gl::FramebufferTexture2D(
		GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture.Get().id_, 0
	));
	PTGN_ASSERT(IsComplete(), "Failed to attach texture to frame buffer");
}

void FrameBufferInstance::AttachRenderBuffer(const RenderBuffer& render_buffer) {
	PTGN_ASSERT(render_buffer.IsValid(), "Cannot attach invalid render buffer to frame buffer");
	PTGN_ASSERT(IsBound(), "Cannot attach render buffer until frame buffer is bound");
	render_buffer_ = render_buffer;
	GLCall(gl::FramebufferRenderbuffer(
		GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, render_buffer.Get().id_
	));
	PTGN_ASSERT(IsComplete(), "Failed to attach render buffer to frame buffer");
}

void FrameBufferInstance::Bind(bool reset_viewport) const {
	FrameBuffer::Bind(id_);
#ifdef PTGN_DEBUG
	++game.stats.frame_buffer_binds;
#endif
	if (reset_viewport && texture_.IsValid()) {
		GLRenderer::SetViewport({}, texture_.GetSize());
	}
}

bool FrameBufferInstance::IsBound() const {
	return FrameBuffer::GetBoundId() == id_;
}

bool FrameBufferInstance::IsComplete() const {
	PTGN_ASSERT(IsBound(), "Cannot check status of frame buffer until it is bound");
	auto status{ GLCallReturn(gl::CheckFramebufferStatus(GL_FRAMEBUFFER)) };
	return status == GL_FRAMEBUFFER_COMPLETE;
	// TODO: Consider adding a way to query frame buffer status.
	// PTGN_ERROR("Incomplete FrameBuffer: ", status);
}

RenderBufferInstance::RenderBufferInstance() {
	GLCall(gl::GenRenderbuffers(1, &id_));
	PTGN_ASSERT(id_ != 0, "Failed to generate render buffer using OpenGL context");
#ifdef GL_ANNOUNCE_RENDER_BUFFER_CALLS
	PTGN_LOG("GL: Generated render buffer with id ", id_);
#endif
}

RenderBufferInstance::~RenderBufferInstance() {
	GLCall(gl::DeleteRenderbuffers(1, &id_));
#ifdef GL_ANNOUNCE_RENDER_BUFFER_CALLS
	PTGN_LOG("GL: Deleted render buffer with id ", id_);
#endif
}

} // namespace impl

RenderBuffer::RenderBuffer(const V2_int& size) {
	auto restore_id{ RenderBuffer::GetBoundId() };

	Create();
	Bind();
	GLCall(gl::RenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, size.x, size.y));

	RenderBuffer::Bind(restore_id);
}

void RenderBuffer::Bind(std::uint32_t id) {
#ifdef GL_ANNOUNCE_RENDER_BUFFER_CALLS
	PTGN_LOG("GL: Bound render buffer with id ", id);
#endif
	GLCall(gl::BindRenderbuffer(GL_RENDERBUFFER, id));
}

void RenderBuffer::Bind() const {
	PTGN_ASSERT(IsValid(), "Cannot bind invalid render buffer");
	Bind(Get().id_);
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

FrameBuffer::FrameBuffer(const Texture& texture, bool rebind_previous_frame_buffer) {
	auto create = [&]() {
		Create();
		Bind();
		Get().AttachTexture(texture);
		PTGN_ASSERT(IsComplete(), "Failed to complete frame buffer");
	};

	if (!rebind_previous_frame_buffer) {
		std::invoke(create);
		return;
	}

	auto restore_id{ FrameBuffer::GetBoundId() };
	std::invoke(create);
	FrameBuffer::Bind(restore_id);
}

FrameBuffer::FrameBuffer(const RenderBuffer& render_buffer, bool rebind_previous_frame_buffer) {
	auto create = [&]() {
		Create();
		Bind();
		Get().AttachRenderBuffer(render_buffer);
		PTGN_ASSERT(IsComplete(), "Failed to complete frame buffer");
	};

	if (!rebind_previous_frame_buffer) {
		std::invoke(create);
		return;
	}

	auto restore_id{ FrameBuffer::GetBoundId() };
	std::invoke(create);
	FrameBuffer::Bind(restore_id);
}

void FrameBuffer::AttachTexture(const Texture& texture) {
	auto restore_id{ FrameBuffer::GetBoundId() };

	Bind();
	Get().AttachTexture(texture);

	FrameBuffer::Bind(restore_id);
}

void FrameBuffer::AttachRenderBuffer(const RenderBuffer& render_buffer) {
	auto restore_id{ FrameBuffer::GetBoundId() };

	Bind();
	Get().AttachRenderBuffer(render_buffer);

	FrameBuffer::Bind(restore_id);
}

Texture FrameBuffer::GetTexture() const {
	PTGN_ASSERT(IsValid(), "Cannot get texture attached to uninitialized frame buffer");
	auto& i{ Get() };
	PTGN_ASSERT(i.texture_.IsValid(), "Cannot get frame buffer texture which has not been set");
	return i.texture_;
}

RenderBuffer FrameBuffer::GetRenderBuffer() const {
	PTGN_ASSERT(IsValid(), "Cannot get render buffer attached to uninitialized frame buffer");
	auto& i{ Get() };
	PTGN_ASSERT(
		i.render_buffer_.IsValid(), "Cannot get frame buffer render buffer which has not been set"
	);
	return i.render_buffer_;
}

bool FrameBuffer::IsComplete() const {
	return IsValid() && Get().IsComplete();
}

bool FrameBuffer::IsBound() const {
	return IsValid() && Get().IsBound();
}

void FrameBuffer::Bind(std::uint32_t id) {
#ifdef GL_ANNOUNCE_FRAME_BUFFER_CALLS
	PTGN_LOG("GL: Bound frame buffer with id ", id);
#endif
	GLCall(gl::BindFramebuffer(GL_FRAMEBUFFER, id));
}

void FrameBuffer::Bind() const {
	PTGN_ASSERT(IsValid(), "Cannot bind invalid frame buffer");
	if (game.renderer.bound_frame_buffer_ == *this) {
		return;
	}
	// Binds frame buffer and resets its viewport.
	Get().Bind(true);
	game.renderer.bound_frame_buffer_ = *this;
}

void FrameBuffer::Unbind() {
	if (game.renderer.bound_frame_buffer_ == FrameBuffer{}) {
		return;
	}
	Bind(0);
	game.renderer.bound_frame_buffer_ = {};
#ifdef PTGN_DEBUG
	++game.stats.frame_buffer_unbinds;
#endif
}

std::uint32_t FrameBuffer::GetBoundId() {
	std::int32_t id{ -1 };
	GLCall(gl::glGetIntegerv(static_cast<gl::GLenum>(impl::GLBinding::FrameBufferDraw), &id));
	PTGN_ASSERT(id >= 0, "Failed to retrieve bound frame buffer id");
	return static_cast<std::uint32_t>(id);
}

} // namespace ptgn