#include "renderer/vertex_array.h"

#include <array>
#include <cstdint>

#include "core/game.h"
#include "renderer/buffer.h"
#include "renderer/buffer_layout.h"
#include "renderer/gl_helper.h"
#include "renderer/gl_loader.h"
#include "renderer/gl_renderer.h"
#include "renderer/renderer.h"
#include "utility/debug.h"
#include "utility/handle.h"
#include "utility/log.h"
#include "utility/stats.h"

namespace ptgn {

namespace impl {

VertexArrayInstance::VertexArrayInstance() {
	GLCall(gl::GenVertexArrays(1, &id_));
	PTGN_ASSERT(id_ != 0, "Failed to generate vertex array using OpenGL context");
#ifdef GL_ANNOUNCE_VERTEX_ARRAY_CALLS
	PTGN_LOG("GL: Generated vertex array with id ", id_);
#endif
}

VertexArrayInstance::~VertexArrayInstance() {
	GLCall(gl::DeleteVertexArrays(1, &id_));
#ifdef GL_ANNOUNCE_VERTEX_ARRAY_CALLS
	PTGN_LOG("GL: Deleted vertex array with id ", id_);
#endif
}

} // namespace impl

std::uint32_t VertexArray::GetBoundId() {
	std::int32_t id{ -1 };
	GLCall(gl::glGetIntegerv(static_cast<gl::GLenum>(impl::GLBinding::VertexArray), &id));
	PTGN_ASSERT(id >= 0, "Failed to retrieve bound vertex array id");
	return static_cast<std::uint32_t>(id);
}

bool VertexArray::WithinMaxAttributes(std::int32_t attribute_count) {
	std::int32_t max_attributes{ 0 };
	GLCall(gl::glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &max_attributes));
	return attribute_count < max_attributes;
}

void VertexArray::Bind() const {
	if (game.renderer.bound_vertex_array_ == *this) {
		return;
	}
	PTGN_ASSERT(IsValid(), "Cannot bind invalid or uninitialized vertex array");
	Bind(Get().id_);
	game.renderer.bound_vertex_array_ = *this;
#ifdef PTGN_DEBUG
	++game.stats.vertex_array_binds;
#endif
}

void VertexArray::Bind(std::uint32_t id) {
	GLCall(gl::BindVertexArray(id));
#ifdef GL_ANNOUNCE_VERTEX_ARRAY_CALLS
	PTGN_LOG("GL: Bound vertex array with id ", id);
#endif
}

void VertexArray::Unbind() {
	if (game.renderer.bound_vertex_array_ == VertexArray{}) {
		return;
	}
	game.renderer.bound_vertex_array_ = {};
#ifndef PTGN_PLATFORM_MACOS
	Bind(0);
#ifdef PTGN_DEBUG
	++game.stats.vertex_array_unbinds;
#endif
#endif
}

void VertexArray::SetVertexBuffer(const VertexBuffer& vertex_buffer) {
	Create();

	Bind();

	SetVertexBufferImpl(vertex_buffer);
}

void VertexArray::SetIndexBuffer(const IndexBuffer& index_buffer) {
	Create();

	Bind();

	SetIndexBufferImpl(index_buffer);
}

void VertexArray::SetVertexBufferImpl(const VertexBuffer& vertex_buffer) {
	auto& v{ Get() };
	PTGN_ASSERT(IsBound());
	PTGN_ASSERT(
		vertex_buffer.IsValid(), "Cannot set vertex buffer which is uninitialized or destroyed"
	);
	vertex_buffer.Bind();
	v.vertex_buffer_ = vertex_buffer;
}

void VertexArray::SetIndexBufferImpl(const IndexBuffer& index_buffer) {
	auto& v{ Get() };
	PTGN_ASSERT(IsBound());
	PTGN_ASSERT(
		index_buffer.IsValid(), "Cannot set index buffer which is uninitialized or destroyed"
	);
	index_buffer.Bind();
	v.index_buffer_ = index_buffer;
}

void VertexArray::SetBufferElement(
	std::uint32_t i, const impl::BufferElement& element, std::int32_t stride
) const {
	GLCall(gl::EnableVertexAttribArray(i));
	if (element.is_integer) {
		GLCall(gl::VertexAttribIPointer(
			i, element.count, static_cast<gl::GLenum>(element.type), stride,
			reinterpret_cast<const void*>(element.offset)
		));
	} else {
		GLCall(gl::VertexAttribPointer(
			i, element.count, static_cast<gl::GLenum>(element.type),
			element.normalized ? static_cast<gl::GLboolean>(GL_TRUE)
							   : static_cast<gl::GLboolean>(GL_FALSE),
			stride, reinterpret_cast<const void*>(element.offset)
		));
	}
}

void VertexArray::Draw(std::size_t index_count, bool bind_vertex_array) const {
	PTGN_ASSERT(IsValid(), "Cannot submit invalid vertex array for rendering");
	PTGN_ASSERT(
		HasVertexBuffer(), "Cannot submit vertex array without a set vertex buffer for rendering"
	);
	if (index_count != 0) {
		PTGN_ASSERT(
			HasIndexBuffer(),
			"Cannot specify index without for a vertex array with no attached index buffer"
		);
		GLRenderer::DrawElements(*this, index_count, bind_vertex_array);
		return;
	}
	if (HasIndexBuffer()) {
		auto count{ GetIndexBuffer().GetCount() };
		PTGN_ASSERT(count > 0, "Cannot draw vertex array with 0 indices");
		GLRenderer::DrawElements(*this, count, bind_vertex_array);
	} else {
		auto count{ GetVertexBuffer().GetCount() };
		PTGN_ASSERT(count > 0, "Cannot draw vertex array with 0 vertices");
		GLRenderer::DrawArrays(*this, count, bind_vertex_array);
	}
}

void VertexArray::SetPrimitiveMode(PrimitiveMode mode) {
	Create();
	Get().mode_ = mode;
}

bool VertexArray::HasVertexBuffer() const {
	return IsValid() && Get().vertex_buffer_.IsValid();
}

bool VertexArray::HasIndexBuffer() const {
	return IsValid() && Get().index_buffer_.IsValid();
}

VertexBuffer VertexArray::GetVertexBuffer() const {
	PTGN_ASSERT(IsValid(), "Cannot get vertex buffer of invalid or uninitialized vertex array");
	return Get().vertex_buffer_;
}

IndexBuffer VertexArray::GetIndexBuffer() const {
	PTGN_ASSERT(IsValid(), "Cannot get index buffer of invalid or uninitialized vertex array");
	return Get().index_buffer_;
}

PrimitiveMode VertexArray::GetPrimitiveMode() const {
	PTGN_ASSERT(IsValid(), "Cannot get primitive mode of invalid or uninitialized vertex array");
	return Get().mode_;
}

bool VertexArray::IsBound() const {
	return IsValid() && GetBoundId() == Get().id_;
}

} // namespace ptgn
