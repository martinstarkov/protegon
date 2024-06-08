#include "vertex_array.h"

#include <cassert>

#include "gl_loader.h"

namespace ptgn {

namespace impl {

VertexArrayInstance::VertexArrayInstance() {
	glGenVertexArrays(1, &id_);
}

VertexArrayInstance::~VertexArrayInstance() {
	glDeleteVertexArrays(1, &id_);
}

} // namespace impl

VertexArray VertexArray::Create() {
	return VertexArray{ std::shared_ptr<impl::VertexArrayInstance>(new impl::VertexArrayInstance()) };
}

VertexArray::VertexArray(const std::shared_ptr<impl::VertexArrayInstance>& instance) : Handle<impl::VertexArrayInstance>{ instance } {}

void VertexArray::Bind() const {
	assert(IsValid() && "Cannot bind uninitialized or destroyed vertex array");
	glBindVertexArray(instance_->id_);
}

void VertexArray::Unbind() const {
	glBindVertexArray(0);
}

void VertexArray::AddVertexBuffer(const VertexBuffer& vertex_buffer) {
	assert(IsValid() && "Cannot add vertex buffer to uninitialized or destroyed vertex array");
	const BufferLayout& layout = vertex_buffer.GetLayout();
	assert(!layout.IsEmpty() && "Cannot add a vertex buffer with an empty (unset) layout to a vertex array");

	glBindVertexArray(instance_->id_);
	vertex_buffer.Bind();

	const std::vector<BufferElement>& elements = layout.GetElements();
	for (std::size_t i = 0; i < elements.size(); ++i) {
		const BufferElement& element{ elements[i] };
		glEnableVertexAttribArray(i);
		glVertexAttribPointer(
			i, element.GetCount(),
			static_cast<GLenum>(element.GetType()),
			element.IsNormalized() ? GL_TRUE : GL_FALSE,
			layout.GetStride(),
			(const void*)element.GetOffset()
		);
		// Not required according to: https://stackoverflow.com/a/12428035
		//glDisableVertexAttribArray(i);
	}
	instance_->vertex_buffers_.push_back(vertex_buffer);
}

void VertexArray::SetIndexBuffer(const IndexBuffer& index_buffer) {
	assert(IsValid() && "Cannot set vertex buffer of uninitialized or destroyed vertex array");
	glBindVertexArray(instance_->id_);
	index_buffer.Bind();
	instance_->index_buffer_ = index_buffer;
}

const IndexBuffer& VertexArray::GetIndexBuffer() const {
	assert(IsValid() && "Cannot get index buffer of uninitialized or destroyed vertex array");
	assert(instance_->index_buffer_.IsValid() && "Cannot get index buffer which is uninitialized or destroyed");
	return instance_->index_buffer_;
}

} // namespace ptgn