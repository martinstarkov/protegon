#include "renderer/vertex_array.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "renderer/buffer.h"
#include "renderer/buffer_layout.h"
#include "renderer/gl_helper.h"
#include "renderer/gl_loader.h"
#include "utility/debug.h"
#include "utility/handle.h"

namespace ptgn {

namespace impl {

VertexArrayInstance::VertexArrayInstance() {
	GLCall(gl::GenVertexArrays(1, &id_));
	PTGN_ASSERT(id_ != 0, "Failed to generate vertex array using OpenGL context");
}

VertexArrayInstance::~VertexArrayInstance() {
	GLCall(gl::DeleteVertexArrays(1, &id_));
}

} // namespace impl

VertexArray::VertexArray(
	PrimitiveMode mode, const VertexBuffer& vertex_buffer, impl::InternalBufferLayout layout,
	const IndexBuffer& index_buffer
) {
	Create();

	SetPrimitiveMode(mode);

	Bind();

	SetVertexBufferImpl(vertex_buffer);

	SetIndexBufferImpl(index_buffer);

	SetLayoutImpl(layout);
}

std::int32_t VertexArray::GetBoundId() {
	std::int32_t id{ -1 };
	GLCall(gl::glGetIntegerv(static_cast<gl::GLenum>(impl::GLBinding::VertexArray), &id));
	PTGN_ASSERT(id >= 0);
	return id;
}

void VertexArray::Bind() const {
	GLCall(gl::BindVertexArray(Get().id_));
}

void VertexArray::Unbind() {
	GLCall(gl::BindVertexArray(0));
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
	PTGN_ASSERT(GetBoundId() == static_cast<std::int32_t>(v.id_));
	PTGN_ASSERT(
		vertex_buffer.IsValid(), "Cannot set vertex buffer which is uninitialized or destroyed"
	);
	vertex_buffer.Bind();
	v.vertex_buffer_ = vertex_buffer;
}

void VertexArray::SetIndexBufferImpl(const IndexBuffer& index_buffer) {
	auto& v{ Get() };
	PTGN_ASSERT(GetBoundId() == static_cast<std::int32_t>(v.id_));
	PTGN_ASSERT(
		index_buffer.IsValid(), "Cannot set index buffer which is uninitialized or destroyed"
	);
	index_buffer.Bind();
	v.index_buffer_ = index_buffer;
}

void VertexArray::SetLayoutImpl(const impl::InternalBufferLayout& layout) const {
	PTGN_ASSERT(
		!layout.IsEmpty(),
		"Cannot add a vertex buffer with an empty (unset) layout to a vertex array"
	);

	const std::vector<impl::BufferElement>& elements = layout.GetElements();

	std::int32_t max_attributes{ 0 };
	GLCall(gl::glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &max_attributes));

	PTGN_ASSERT(elements.size() < max_attributes, "Too many vertex attributes");

	for (std::uint32_t i = 0; i < elements.size(); ++i) {
		const impl::BufferElement& element{ elements[i] };
		GLCall(gl::EnableVertexAttribArray(i));
		if (element.is_integer) {
			GLCall(gl::VertexAttribIPointer(
				i, element.count, static_cast<gl::GLenum>(element.type), layout.GetStride(),
				(const void*)element.offset
			));
		} else {
			GLCall(gl::VertexAttribPointer(
				i, element.count, static_cast<gl::GLenum>(element.type),
				element.normalized ? GL_TRUE : GL_FALSE, layout.GetStride(),
				(const void*)element.offset
			));
		}
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

VertexBuffer VertexArray::GetVertexBuffer() {
	return Get().vertex_buffer_;
}

IndexBuffer VertexArray::GetIndexBuffer() {
	return Get().index_buffer_;
}

PrimitiveMode VertexArray::GetPrimitiveMode() const {
	return Get().mode_;
}

} // namespace ptgn