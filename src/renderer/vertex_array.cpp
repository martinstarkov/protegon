#include "protegon/vertex_array.h"

#include "renderer/gl_loader.h"
#include "utility/debug.h"

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
	PrimitiveMode mode, const VertexBuffer& vertex_buffer, const impl::InternalBufferLayout& layout,
	const IndexBuffer& index_buffer
) {
	if (!IsValid()) {
		instance_ = std::make_shared<impl::VertexArrayInstance>();
	}

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
	PTGN_ASSERT(IsValid(), "Cannot bind uninitialized or destroyed vertex array");
	GLCall(gl::BindVertexArray(instance_->id_));
}

void VertexArray::Unbind() {
	GLCall(gl::BindVertexArray(0));
}

void VertexArray::SetVertexBuffer(const VertexBuffer& vertex_buffer) {
	if (!IsValid()) {
		instance_ = std::make_shared<impl::VertexArrayInstance>();
	}

	Bind();

	SetVertexBufferImpl(vertex_buffer);
}

void VertexArray::SetIndexBuffer(const IndexBuffer& index_buffer) {
	if (!IsValid()) {
		instance_ = std::make_shared<impl::VertexArrayInstance>();
	}

	Bind();

	SetIndexBufferImpl(index_buffer);
}

void VertexArray::SetVertexBufferImpl(const VertexBuffer& vertex_buffer) {
	PTGN_ASSERT(IsValid(), "Cannot add vertex buffer to uninitialized or destroyed vertex array");
	PTGN_ASSERT(GetBoundId() == static_cast<std::int32_t>(instance_->id_));
	PTGN_ASSERT(
		vertex_buffer.IsValid(), "Cannot set vertex buffer which is uninitialized or destroyed"
	);
	vertex_buffer.Bind();
	instance_->vertex_buffer_ = vertex_buffer;
}

void VertexArray::SetIndexBufferImpl(const IndexBuffer& index_buffer) {
	PTGN_ASSERT(IsValid(), "Cannot set index buffer of uninitialized or destroyed vertex array");
	PTGN_ASSERT(GetBoundId() == static_cast<std::int32_t>(instance_->id_));
	PTGN_ASSERT(
		index_buffer.IsValid(), "Cannot set index buffer which is uninitialized or destroyed"
	);
	index_buffer.Bind();
	instance_->index_buffer_ = index_buffer;
}

void VertexArray::SetLayoutImpl(const impl::InternalBufferLayout& layout) {
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
		GLCall(gl::VertexAttribPointer(
			i, element.count, static_cast<gl::GLenum>(element.type),
			element.normalized ? GL_TRUE : GL_FALSE, layout.GetStride(), (const void*)element.offset
		));
	}
}

void VertexArray::SetPrimitiveMode(PrimitiveMode mode) {
	if (!IsValid()) {
		instance_ = std::make_shared<impl::VertexArrayInstance>();
	}
	instance_->mode_ = mode;
}

bool VertexArray::HasVertexBuffer() const {
	return IsValid() && instance_->vertex_buffer_.IsValid();
}

bool VertexArray::HasIndexBuffer() const {
	return IsValid() && instance_->index_buffer_.IsValid();
}

VertexBuffer VertexArray::GetVertexBuffer() {
	PTGN_ASSERT(IsValid(), "Cannot get vertex buffer of uninitialized or destroyed vertex array");
	return instance_->vertex_buffer_;
}

IndexBuffer VertexArray::GetIndexBuffer() {
	PTGN_ASSERT(IsValid(), "Cannot get index buffer of uninitialized or destroyed vertex array");
	return instance_->index_buffer_;
}

PrimitiveMode VertexArray::GetPrimitiveMode() const {
	PTGN_ASSERT(IsValid(), "Cannot get primitive mode of uninitialized or destroyed vertex array");
	return instance_->mode_;
}

} // namespace ptgn