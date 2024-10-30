#include "renderer/frame_buffer.h"

#include "math/vector2.h"
#include "renderer/gl_helper.h"
#include "renderer/gl_loader.h"
#include "renderer/texture.h"
#include "utility/debug.h"

namespace ptgn {

#define PUSHSTATE_RB()             \
	std::int32_t restore_id {      \
		RenderBuffer::GetBoundId() \
	}
#define POPSTATE_RB() GLCall(gl::BindRenderbuffer(GL_RENDERBUFFER, restore_id))

#define PUSHSTATE_FB()            \
	std::int32_t restore_id {     \
		FrameBuffer::GetBoundId() \
	}
#define POPSTATE_FB() GLCall(gl::BindFramebuffer(GL_FRAMEBUFFER, restore_id))

namespace impl {

FrameBufferInstance::FrameBufferInstance() {
	GLCall(gl::GenFramebuffers(1, &id_));
	PTGN_ASSERT(id_ != 0, "Failed to generate frame buffer using OpenGL context");
}

FrameBufferInstance::~FrameBufferInstance() {
	GLCall(gl::DeleteFramebuffers(1, &id_));
}

RenderBufferInstance::RenderBufferInstance() {
	GLCall(gl::GenRenderbuffers(1, &id_));
	PTGN_ASSERT(id_ != 0, "Failed to generate render buffer using OpenGL context");
}

RenderBufferInstance::~RenderBufferInstance() {
	GLCall(gl::DeleteRenderbuffers(1, &id_));
}

} // namespace impl

RenderBuffer::RenderBuffer(const V2_int& size) {
	PUSHSTATE_RB();

	Create();
	Bind();
	GLCall(gl::RenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, size.x, size.y));

	POPSTATE_FB();
}

void RenderBuffer::Bind() const {
	PTGN_ASSERT(IsValid(), "Cannot bind invalid render buffer");
	GLCall(gl::BindRenderbuffer(GL_RENDERBUFFER, Get().id_));
}

void RenderBuffer::Unbind() {
	GLCall(gl::BindRenderbuffer(GL_RENDERBUFFER, 0));
}

std::int32_t RenderBuffer::GetBoundId() {
	std::int32_t id{ -1 };
	GLCall(gl::glGetIntegerv(static_cast<gl::GLenum>(impl::GLBinding::RenderBuffer), &id));
	PTGN_ASSERT(id >= 0, "Unrecognized type for bound id check");
	return id;
}

FrameBuffer::FrameBuffer(const Texture& texture) {
	PUSHSTATE_FB();

	Create();
	Bind();
	AttachTextureImpl(texture);

	PTGN_ASSERT(IsComplete(), "Failed to complete frame buffer");

	POPSTATE_FB();
}

FrameBuffer::FrameBuffer(const RenderBuffer& render_buffer) {
	PUSHSTATE_FB();

	Create();
	Bind();
	AttachRenderBufferImpl(render_buffer);

	PTGN_ASSERT(IsComplete(), "Failed to complete frame buffer");

	POPSTATE_FB();
}

FrameBuffer::FrameBuffer(const Texture& texture, const RenderBuffer& render_buffer) {
	PUSHSTATE_FB();

	Create();
	Bind();
	AttachTextureImpl(texture);
	AttachRenderBufferImpl(render_buffer);

	PTGN_ASSERT(IsComplete(), "Failed to complete frame buffer");

	POPSTATE_FB();
}

void FrameBuffer::AttachTextureImpl(const Texture& texture) const {
	PTGN_ASSERT(texture.IsValid(), "Cannot attach invalid texture to frame buffer");
	PTGN_ASSERT(
		GetBoundId() == static_cast<std::int32_t>(Get().id_),
		"Cannot attach texture until frame buffer is bound"
	);
	GLCall(gl::FramebufferTexture2D(
		GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture.Get().id_, 0
	));
}

void FrameBuffer::AttachRenderBufferImpl(const RenderBuffer& render_buffer) const {
	PTGN_ASSERT(render_buffer.IsValid(), "Cannot attach invalid render buffer to frame buffer");
	PTGN_ASSERT(
		GetBoundId() == static_cast<std::int32_t>(Get().id_),
		"Cannot attach render buffer until frame buffer is bound"
	);
	GLCall(gl::FramebufferRenderbuffer(
		GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, render_buffer.Get().id_
	));
}

void FrameBuffer::AttachTexture(const Texture& texture) const {
	PUSHSTATE_FB();

	Bind();
	AttachTextureImpl(texture);

	POPSTATE_FB();
}

void FrameBuffer::AttachRenderBuffer(const RenderBuffer& render_buffer) const {
	PUSHSTATE_FB();

	Bind();
	AttachRenderBufferImpl(render_buffer);

	POPSTATE_FB();
}

bool FrameBuffer::IsComplete() const {
	PTGN_ASSERT(
		GetBoundId() == static_cast<std::int32_t>(Get().id_),
		"Cannot checkk status of frame buffer until it is bound"
	);
	auto status{ GLCallReturn(gl::CheckFramebufferStatus(GL_FRAMEBUFFER)) };
	return status == GL_FRAMEBUFFER_COMPLETE;
	// TODO: Consider adding a way to query status.
	// PTGN_ERROR("Incomplete FrameBuffer: ", status);
}

void FrameBuffer::Bind() const {
	PTGN_ASSERT(IsValid(), "Cannot bind invalid frame buffer");
	GLCall(gl::BindFramebuffer(GL_FRAMEBUFFER, Get().id_));
}

void FrameBuffer::Unbind() {
	GLCall(gl::BindFramebuffer(GL_FRAMEBUFFER, 0));
}

std::int32_t FrameBuffer::GetBoundId() {
	std::int32_t id{ -1 };
	GLCall(gl::glGetIntegerv(static_cast<gl::GLenum>(impl::GLBinding::FrameBufferDraw), &id));
	PTGN_ASSERT(id >= 0, "Unrecognized type for bound id check");
	return id;
}

} // namespace ptgn