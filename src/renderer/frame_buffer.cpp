#include "renderer/frame_buffer.h"

#include <cstdint>
#include <functional>
#include <type_traits>
#include <vector>

#include "core/game.h"
#include "core/window.h"
#include "event/event_handler.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/gl_helper.h"
#include "renderer/gl_loader.h"
#include "renderer/gl_renderer.h"
#include "renderer/renderer.h"
#include "renderer/texture.h"
#include "utility/debug.h"
#include "utility/handle.h"

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

FrameBufferInstance::FrameBufferInstance(const V2_float& size) : FrameBufferInstance() {
	CreateBlank(size);
}

FrameBufferInstance::FrameBufferInstance(bool resize_with_window) : FrameBufferInstance() {
	if (resize_with_window) {
		game.event.window.Subscribe(
			WindowEvent::Resized, this,
			std::function([this](const WindowResizedEvent&) { CreateBlank(game.window.GetSize()); })
		);
	}
	CreateBlank(game.window.GetSize());
}

FrameBufferInstance::~FrameBufferInstance() {
	GLCall(gl::DeleteFramebuffers(1, &id_));
}

void FrameBufferInstance::CreateBlank(const V2_float& size) {
	Bind();
	AttachTexture(Texture{ nullptr, size, ImageFormat::RGBA8888, TextureWrapping::ClampEdge,
						   TextureFilter::Nearest, TextureFilter::Nearest, false });
	AttachRenderBuffer(RenderBuffer{ size });
	PTGN_ASSERT(IsComplete());
}

void FrameBufferInstance::AttachTexture(const Texture& texture) {
	PTGN_ASSERT(texture.IsValid(), "Cannot attach invalid texture to frame buffer");
	PTGN_ASSERT(IsBound(), "Cannot attach texture until frame buffer is bound");
	texture_ = texture;
	GLCall(gl::FramebufferTexture2D(
		GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture.Get().id_, 0
	));
}

void FrameBufferInstance::AttachRenderBuffer(const RenderBuffer& render_buffer) {
	PTGN_ASSERT(render_buffer.IsValid(), "Cannot attach invalid render buffer to frame buffer");
	PTGN_ASSERT(IsBound(), "Cannot attach render buffer until frame buffer is bound");
	render_buffer_ = render_buffer;
	GLCall(gl::FramebufferRenderbuffer(
		GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, render_buffer.Get().id_
	));
}

void FrameBufferInstance::Bind() const {
	FrameBuffer::Bind(id_);
#ifdef PTGN_DEBUG
	++game.stats.frame_buffer_binds;
#endif
	if (texture_.IsValid()) {
		GLRenderer::SetViewport({}, texture_.GetSize());
	}
}

bool FrameBufferInstance::IsBound() const {
	return FrameBuffer::GetBoundId() == static_cast<std::int32_t>(id_);
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

	POPSTATE_RB();
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

FrameBuffer::FrameBuffer(bool continuously_window_sized) {
	PUSHSTATE_FB();

	Create(continuously_window_sized);

	POPSTATE_FB();
}

FrameBuffer::FrameBuffer(const V2_float& size) {
	PUSHSTATE_FB();

	Create(size);

	POPSTATE_FB();
}

FrameBuffer::FrameBuffer(const Texture& texture) {
	PUSHSTATE_FB();

	Create();
	Bind();
	Get().AttachTexture(texture);

	PTGN_ASSERT(IsComplete(), "Failed to complete frame buffer");

	POPSTATE_FB();
}

FrameBuffer::FrameBuffer(const RenderBuffer& render_buffer) {
	PUSHSTATE_FB();

	Create();
	Bind();
	Get().AttachRenderBuffer(render_buffer);

	PTGN_ASSERT(IsComplete(), "Failed to complete frame buffer");

	POPSTATE_FB();
}

FrameBuffer::FrameBuffer(const Texture& texture, const RenderBuffer& render_buffer) {
	PUSHSTATE_FB();

	Create();
	Bind();
	auto& i{ Get() };
	i.AttachTexture(texture);
	i.AttachRenderBuffer(render_buffer);

	PTGN_ASSERT(IsComplete(), "Failed to complete frame buffer");

	POPSTATE_FB();
}

void FrameBuffer::ClearToColor(const Color& clear_color) {
	PTGN_ASSERT(IsBound(), "Frame buffer / render target must be bound before clearing to a color");
	auto og_cc{ game.renderer.GetClearColor() };
	game.renderer.SetClearColor(clear_color);
	game.renderer.Clear();
	game.renderer.SetClearColor(og_cc);
}

void FrameBuffer::WhileBound(
	const std::function<void()>& draw_callback, const Color& clear_color, BlendMode blend_mode
) {
	FrameBuffer restore{ game.renderer.bound_ };

	Bind();
	ClearToColor(clear_color);
	auto og_blend{ game.renderer.GetBlendMode() };
	game.renderer.SetBlendMode(blend_mode);
	std::invoke(draw_callback);
	game.renderer.SetBlendMode(og_blend);

	if (restore.IsValid()) {
		restore.Bind();
	} else {
		game.renderer.screen_.Bind();
	}
}

void FrameBuffer::AttachTexture(const Texture& texture) {
	PUSHSTATE_FB();

	Bind();
	Get().AttachTexture(texture);

	POPSTATE_FB();
}

void FrameBuffer::AttachRenderBuffer(const RenderBuffer& render_buffer) {
	PUSHSTATE_FB();

	Bind();
	Get().AttachRenderBuffer(render_buffer);

	POPSTATE_FB();
}

V2_int FrameBuffer::GetSize() const {
	PTGN_ASSERT(IsValid(), "Cannot get size of uninitialized frame buffer");
	auto& i{ Get() };
	PTGN_ASSERT(
		i.texture_.IsValid(), "Cannot get frame buffer size if its texture has not been set"
	);
	return i.texture_.GetSize();
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

void FrameBuffer::Bind() const {
	PTGN_ASSERT(IsValid(), "Cannot bind invalid frame buffer");
	Get().Bind();
	game.renderer.bound_ = *this;
}

void FrameBuffer::Unbind() {
	FrameBuffer::Bind(0);
	game.renderer.bound_ = FrameBuffer{};
#ifdef PTGN_DEBUG
	++game.stats.frame_buffer_unbinds;
#endif
}

void FrameBuffer::Draw(
	const Rect& destination, const M4_float& view_projection, const TextureInfo& texture_info
) const {
	game.shader.Get(ScreenShader::Default)
		.Draw(GetTexture(), destination, view_projection, texture_info);
}

void FrameBuffer::DrawToScreen(
	const Rect& destination, const M4_float& view_projection, const TextureInfo& texture_info
) const {
	FrameBuffer bound{ game.renderer.bound_ };

	game.renderer.screen_.Bind();

	Draw(destination, view_projection, texture_info);

	bound.Bind();
}

Color FrameBuffer::GetPixel(const V2_int& coordinate) const {
	PTGN_ASSERT(IsValid(), "Cannot retrieve pixel of invalid frame buffer");
	auto& texture{ Get().texture_ };
	PUSHSTATE_FB();
	Bind();
	auto color{ texture.GetPixel(coordinate) };
	POPSTATE_FB();
	return color;
}

void FrameBuffer::ForEachPixel(const std::function<void(V2_int, Color)>& func) const {
	PTGN_ASSERT(IsValid(), "Cannot retrieve pixels of invalid frame buffer");
	auto& texture{ Get().texture_ };
	PUSHSTATE_FB();
	Bind();
	texture.ForEachPixel(func);
	POPSTATE_FB();
}

std::int32_t FrameBuffer::GetBoundId() {
	std::int32_t id{ -1 };
	GLCall(gl::glGetIntegerv(static_cast<gl::GLenum>(impl::GLBinding::FrameBufferDraw), &id));
	PTGN_ASSERT(id >= 0, "Unrecognized type for bound id check");
	return id;
}

void FrameBuffer::Bind(std::uint32_t id) {
	GLCall(gl::BindFramebuffer(GL_FRAMEBUFFER, id));
}

} // namespace ptgn