#include "renderer/buffers/vertex_array.h"

#include <array>
#include <cstdint>

#include "common/assert.h"
#include "core/game.h"
#include "debug/config.h"
#include "debug/debug_system.h"
#include "debug/stats.h"
#include "renderer/buffers/buffer.h"
#include "renderer/buffers/buffer_layout.h"
#include "renderer/gl/gl_helper.h"
#include "renderer/gl/gl_loader.h"
#include "renderer/renderer.h"

namespace ptgn::impl {

VertexArray::VertexArray(VertexArray&& other) noexcept :
	id_{ std::exchange(other.id_, 0) },
	mode_{ other.mode_ },
	vertex_buffer_{ std::exchange(other.vertex_buffer_, {}) },
	index_buffer_{ std::exchange(other.index_buffer_, {}) } {}

VertexArray& VertexArray::operator=(VertexArray&& other) noexcept {
	if (this != &other) {
		DeleteVertexArray();
		id_			   = std::exchange(other.id_, 0);
		mode_		   = other.mode_;
		vertex_buffer_ = std::exchange(other.vertex_buffer_, {});
		index_buffer_  = std::exchange(other.index_buffer_, {});
	}
	return *this;
}

VertexArray::~VertexArray() {
	DeleteVertexArray();
}

void VertexArray::GenerateVertexArray() {
	GLCall(GenVertexArrays(1, &id_));
	PTGN_ASSERT(IsValid(), "Failed to generate vertex array using OpenGL context");
#ifdef GL_ANNOUNCE_VERTEX_ARRAY_CALLS
	PTGN_LOG("GL: Generated vertex array with id ", id_);
#endif
}

void VertexArray::DeleteVertexArray() noexcept {
	if (!IsValid()) {
		return;
	}
	GLCall(DeleteVertexArrays(1, &id_));
#ifdef GL_ANNOUNCE_VERTEX_ARRAY_CALLS
	PTGN_LOG("GL: Deleted vertex array with id ", id_);
#endif
	id_ = 0;
}

std::uint32_t VertexArray::GetBoundId() {
	std::int32_t id{ -1 };
	GLCall(glGetIntegerv(static_cast<GLenum>(GLBinding::VertexArray), &id));
	PTGN_ASSERT(id >= 0, "Failed to retrieve bound vertex array id");
	return static_cast<std::uint32_t>(id);
}

std::uint32_t VertexArray::GetMaxAttributes() {
	std::int32_t max_attributes{ 0 };
	GLCall(glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &max_attributes));
	PTGN_ASSERT(max_attributes >= 0, "Failed to retrieve max vertex attributes");
	return static_cast<std::uint32_t>(max_attributes);
}

void VertexArray::Bind() const {
	PTGN_ASSERT(IsValid(), "Cannot bind destroyed or uninitialized vertex array");
	Bind(id_);
}

void VertexArray::Bind(std::uint32_t id) {
	if (game.renderer.bound_.vertex_array_id == id) {
		return;
	}
#ifdef PTGN_PLATFORM_MACOS
	// MacOS complains about binding 0 id vertex array.
	if (id == 0) {
		return;
	}
#endif
	GLCall(BindVertexArray(id));
	game.renderer.bound_.vertex_array_id = id;
#ifdef PTGN_DEBUG
	++game.debug.stats.vertex_array_binds;
#endif
#ifdef GL_ANNOUNCE_VERTEX_ARRAY_CALLS
	PTGN_LOG("GL: Bound vertex array with id ", id);
#endif
}

void VertexArray::Unbind() {
	Bind(0);
}

void VertexArray::SetPrimitiveMode(PrimitiveMode mode) {
	mode_ = mode;
}

void VertexArray::SetVertexBuffer(VertexBuffer&& vertex_buffer) {
	PTGN_ASSERT(IsBound(), "Vertex array must be bound before setting vertex buffer");
	PTGN_ASSERT(vertex_buffer.IsValid(), "Cannot set vertex buffer which is uninitialized");
	vertex_buffer_ = std::move(vertex_buffer);
	vertex_buffer_.Bind();
}

void VertexArray::SetIndexBuffer(IndexBuffer&& index_buffer) {
	PTGN_ASSERT(IsBound(), "Vertex array must be bound before setting index buffer");
	PTGN_ASSERT(index_buffer.IsValid(), "Cannot set index buffer which is uninitialized");
	index_buffer_ = std::move(index_buffer);
	index_buffer_.Bind();
}

void VertexArray::SetBufferElement(
	std::uint32_t i, const BufferElement& element, std::int32_t stride
) const {
	GLCall(EnableVertexAttribArray(i));
	if (element.is_integer) {
		GLCall(VertexAttribIPointer(
			i, element.count, static_cast<GLenum>(element.type), stride,
			reinterpret_cast<const void*>(element.offset)
		));
		return;
	}
	GLCall(VertexAttribPointer(
		i, element.count, static_cast<GLenum>(element.type),
		element.normalized ? static_cast<GLboolean>(GL_TRUE) : static_cast<GLboolean>(GL_FALSE),
		stride, reinterpret_cast<const void*>(element.offset)
	));
}

bool VertexArray::HasVertexBuffer() const {
	return vertex_buffer_.IsValid();
}

bool VertexArray::HasIndexBuffer() const {
	return index_buffer_.IsValid();
}

const VertexBuffer& VertexArray::GetVertexBuffer() const {
	return vertex_buffer_;
}

VertexBuffer& VertexArray::GetVertexBuffer() {
	return vertex_buffer_;
}

const IndexBuffer& VertexArray::GetIndexBuffer() const {
	return index_buffer_;
}

IndexBuffer& VertexArray::GetIndexBuffer() {
	return index_buffer_;
}

PrimitiveMode VertexArray::GetPrimitiveMode() const {
	return mode_;
}

bool VertexArray::IsBound() const {
	return GetBoundId() == id_;
}

bool VertexArray::IsValid() const {
	return id_;
}

} // namespace ptgn::impl