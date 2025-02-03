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

VertexArray::VertexArray() {
	GLCall(gl::GenVertexArrays(1, &id));
	PTGN_ASSERT(id != 0, "Failed to generate vertex array using OpenGL context");
#ifdef GL_ANNOUNCE_VERTEX_ARRAY_CALLS
	PTGN_LOG("GL: Generated vertex array with id ", id);
#endif
}

VertexArray::~VertexArray() {
	GLCall(gl::DeleteVertexArrays(1, &id));
#ifdef GL_ANNOUNCE_VERTEX_ARRAY_CALLS
	PTGN_LOG("GL: Deleted vertex array with id ", id);
#endif
}

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
	Bind(id);
}

void VertexArray::Bind(std::uint32_t id) {
	if (game.renderer.bound_vertex_array_id_ == id) {
		return;
	}
	game.renderer.bound_vertex_array_id_ = id;
	GLCall(gl::BindVertexArray(id));
#ifdef PTGN_DEBUG
	++game.stats.vertex_array_binds;
#endif
#ifdef GL_ANNOUNCE_VERTEX_ARRAY_CALLS
	PTGN_LOG("GL: Bound vertex array with id ", id);
#endif
}

void VertexArray::Unbind() {
#ifndef PTGN_PLATFORM_MACOS
	Bind(0);
#endif
}

void VertexArray::SetVertexBuffer(std::unique_ptr<VertexBuffer> new_vertex_buffer) {
	Bind();
	SetVertexBufferImpl(std::move(new_vertex_buffer));
}

void VertexArray::SetIndexBuffer(std::unique_ptr<IndexBuffer> new_index_buffer) {
	Bind();
	SetIndexBufferImpl(std::move(new_index_buffer));
}

void VertexArray::SetVertexBufferImpl(std::unique_ptr<VertexBuffer> new_vertex_buffer) {
	PTGN_ASSERT(IsBound());
	PTGN_ASSERT(
		new_vertex_buffer != nullptr, "Cannot set vertex buffer which is uninitialized or destroyed"
	);
	new_vertex_buffer->Bind();
	vertex_buffer = std::move(new_vertex_buffer);
}

void VertexArray::SetIndexBufferImpl(std::unique_ptr<IndexBuffer> new_index_buffer) {
	PTGN_ASSERT(IsBound());
	PTGN_ASSERT(
		new_index_buffer != nullptr, "Cannot set index buffer which is uninitialized or destroyed"
	);
	new_index_buffer->Bind();
	this->index_buffer = std::move(new_index_buffer);
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
		auto count{ index_buffer->GetElementCount() };
		PTGN_ASSERT(count > 0, "Cannot draw vertex array with 0 indices");
		GLRenderer::DrawElements(*this, count, bind_vertex_array);
	} else {
		auto count{ vertex_buffer->GetElementCount() };
		PTGN_ASSERT(count > 0, "Cannot draw vertex array with 0 vertices");
		GLRenderer::DrawArrays(*this, count, bind_vertex_array);
	}
}

void VertexArray::SetPrimitiveMode(PrimitiveMode new_mode) {
	mode = new_mode;
}

bool VertexArray::HasVertexBuffer() const {
	return vertex_buffer != nullptr;
}

bool VertexArray::HasIndexBuffer() const {
	return index_buffer != nullptr;
}

std::unique_ptr<VertexBuffer>& VertexArray::GetVertexBuffer() {
	return vertex_buffer;
}

std::unique_ptr<IndexBuffer>& VertexArray::GetIndexBuffer() {
	return index_buffer;
}

PrimitiveMode VertexArray::GetPrimitiveMode() const {
	return mode;
}

bool VertexArray::IsBound() const {
	return GetBoundId() == id;
}

} // namespace impl

} // namespace ptgn
