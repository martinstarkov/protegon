#include "renderer/frame_buffer.h"

#include <cstdint>
#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

#include "core/game.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/gl_helper.h"
#include "renderer/gl_loader.h"
#include "renderer/gl_types.h"
#include "renderer/renderer.h"
#include "renderer/texture.h"
#include "utility/assert.h"
#include "utility/debug.h"
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
	id_ = 0;
}

void RenderBuffer::Bind(std::uint32_t id) {
	GLCall(gl::BindRenderbuffer(GL_RENDERBUFFER, id));
#ifdef GL_ANNOUNCE_RENDER_BUFFER_CALLS
	PTGN_LOG("GL: Bound render buffer with id ", id);
#endif
}

void RenderBuffer::Bind() const {
	PTGN_ASSERT(IsValid(), "Cannot bind destroyed or uninitialized render buffer");
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
	id_ = 0;
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

Texture& FrameBuffer::GetTexture() {
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

Color FrameBuffer::GetPixel(const V2_int& coordinate) const {
	V2_int size{ texture_.GetSize() };
	PTGN_ASSERT(
		coordinate.x >= 0 && coordinate.x < size.x,
		"Cannot get pixel out of range of frame buffer texture"
	);
	PTGN_ASSERT(
		coordinate.y >= 0 && coordinate.y < size.y,
		"Cannot get pixel out of range of frame buffer texture"
	);
	auto formats{ impl::GetGLFormats(texture_.GetFormat()) };
	PTGN_ASSERT(
		formats.color_components >= 3,
		"Textures with less than 3 pixel components cannot currently be queried"
	);
	std::vector<std::uint8_t> v(static_cast<std::size_t>(formats.color_components * 1 * 1));
	int y{ size.y - 1 - coordinate.y };
	PTGN_ASSERT(y >= 0);
	Bind();
	GLCall(gl::glReadPixels(
		coordinate.x, y, 1, 1, static_cast<gl::GLenum>(formats.input_format),
		static_cast<gl::GLenum>(impl::GLType::UnsignedByte), static_cast<void*>(v.data())
	));
	return Color{ v[0], v[1], v[2],
				  formats.color_components == 4 ? v[3] : static_cast<std::uint8_t>(255) };
}

void FrameBuffer::ForEachPixel(const std::function<void(V2_int, Color)>& func) const {
	V2_int size{ texture_.GetSize() };
	auto formats{ impl::GetGLFormats(texture_.GetFormat()) };
	PTGN_ASSERT(
		formats.color_components >= 3,
		"Textures with less than 3 pixel components cannot currently be queried"
	);

	std::vector<std::uint8_t> v(static_cast<std::size_t>(formats.color_components * size.x * size.y)
	);
	Bind();
	GLCall(gl::glReadPixels(
		0, 0, size.x, size.y, static_cast<gl::GLenum>(formats.input_format),
		static_cast<gl::GLenum>(impl::GLType::UnsignedByte), static_cast<void*>(v.data())
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
			std::invoke(func, V2_int{ i, j }, color);
		}
	}
}

} // namespace ptgn::impl