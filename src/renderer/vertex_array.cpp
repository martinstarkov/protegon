#include "protegon/vertex_array.h"

#include "renderer/gl_loader.h"
#include "utility/debug.h"

namespace ptgn {

namespace impl {

VertexArrayInstance::VertexArrayInstance() {
	gl::GenVertexArrays(1, &id_);
	PTGN_ASSERT(id_ != 0, "Failed to generate vertex array using OpenGL context");
}

VertexArrayInstance::~VertexArrayInstance() {
	gl::DeleteVertexArrays(1, &id_);
}

} // namespace impl

VertexArray::VertexArray(
	PrimitiveMode mode, const VertexBuffer& vertex_buffer, const IndexBuffer& index_buffer
) {
	if (!IsValid()) {
		instance_ = std::make_shared<impl::VertexArrayInstance>();
	}

	SetPrimitiveMode(mode);

	Bind();

	if (vertex_buffer.IsValid()) {
		SetVertexBufferImpl(vertex_buffer);
	}

	if (index_buffer.IsValid()) {
		SetIndexBufferImpl(index_buffer);
	}
}

std::int32_t VertexArray::BoundId() {
	std::int32_t id{ 0 };
	gl::glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &id);
	PTGN_ASSERT(id >= 0);
	return id;
}

void VertexArray::Bind() const {
	PTGN_ASSERT(IsValid(), "Cannot bind uninitialized or destroyed vertex array");
	gl::BindVertexArray(instance_->id_);
}

void VertexArray::Unbind() {
	gl::BindVertexArray(0);
}

void VertexArray::SetVertexBuffer(const VertexBuffer& vertex_buffer) {
	if (!IsValid()) {
		instance_ = std::make_shared<impl::VertexArrayInstance>();
	}

	Bind();

	SetVertexBufferImpl(vertex_buffer);
}

void VertexArray::SetVertexBufferImpl(const VertexBuffer& vertex_buffer) {
	PTGN_ASSERT(IsValid(), "Cannot add vertex buffer to uninitialized or destroyed vertex array");
	PTGN_ASSERT(static_cast<std::uint32_t>(BoundId()) == instance_->id_);
	PTGN_ASSERT(
		vertex_buffer.IsValid(), "Cannot set vertex buffer which is uninitialized or destroyed"
	);
	PTGN_ASSERT(
		!vertex_buffer.instance_->layout_.IsEmpty(),
		"Cannot add a vertex buffer with an empty (unset) layout to a vertex array"
	);

	vertex_buffer.Bind();

	const std::vector<impl::BufferElement>& elements =
		vertex_buffer.instance_->layout_.GetElements();

	std::int32_t max_attributes{ 0 };
	gl::glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &max_attributes);

	PTGN_ASSERT(elements.size() < max_attributes, "Too many vertex attributes");

	for (std::uint32_t i = 0; i < elements.size(); ++i) {
		const impl::BufferElement& element{ elements[i] };
		gl::EnableVertexAttribArray(i);
		gl::VertexAttribPointer(
			i, element.count, static_cast<gl::GLenum>(element.type),
			element.normalized ? GL_TRUE : GL_FALSE, vertex_buffer.instance_->layout_.GetStride(),
			(const void*)element.offset
		);
	}

	instance_->vertex_buffer_ = vertex_buffer;
}

void VertexArray::SetIndexBuffer(const IndexBuffer& index_buffer) {
	if (!IsValid()) {
		instance_ = std::make_shared<impl::VertexArrayInstance>();
	}

	Bind();

	SetIndexBufferImpl(index_buffer);
}

void VertexArray::SetIndexBufferImpl(const IndexBuffer& index_buffer) {
	PTGN_ASSERT(IsValid(), "Cannot set index buffer of uninitialized or destroyed vertex array");
	PTGN_ASSERT(
		index_buffer.IsValid(), "Cannot set index buffer which is uninitialized or destroyed"
	);
	PTGN_ASSERT(static_cast<std::uint32_t>(BoundId()) == instance_->id_);
	index_buffer.Bind();
	instance_->index_buffer_ = index_buffer;
}

void VertexArray::SetPrimitiveMode(PrimitiveMode mode) {
	if (!IsValid()) {
		instance_ = std::make_shared<impl::VertexArrayInstance>();
	}
	PTGN_ASSERT(IsValid(), "Cannot set primitive mode of uninitialized or destroyed vertex array");
	instance_->mode_ = mode;
}

const VertexBuffer& VertexArray::GetVertexBuffer() const {
	PTGN_ASSERT(IsValid(), "Cannot get vertex buffer of uninitialized or destroyed vertex array");
	return instance_->vertex_buffer_;
}

const IndexBuffer& VertexArray::GetIndexBuffer() const {
	PTGN_ASSERT(IsValid(), "Cannot get index buffer of uninitialized or destroyed vertex array");
	return instance_->index_buffer_;
}

PrimitiveMode VertexArray::GetPrimitiveMode() const {
	PTGN_ASSERT(IsValid(), "Cannot get primitive mode of uninitialized or destroyed vertex array");
	return instance_->mode_;
}

} // namespace ptgn