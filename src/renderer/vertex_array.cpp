#include "protegon/vertex_array.h"

#include "gl_loader.h"

#include "utility/debug.h"

namespace ptgn {

namespace impl {

VertexArrayInstance::~VertexArrayInstance() {
	glDeleteVertexArrays(1, &id_);
}

VertexArrayInstance::VertexArrayInstance(PrimitiveMode mode) : mode_{ mode } {
	glGenVertexArrays(1, &id_);
}

} // namespace impl

VertexArray::VertexArray(PrimitiveMode mode) {
	instance_ = std::shared_ptr<impl::VertexArrayInstance>(new impl::VertexArrayInstance(mode));
}

VertexArray::VertexArray(const std::shared_ptr<impl::VertexArrayInstance>& instance) : Handle<impl::VertexArrayInstance>{ instance } {}

void VertexArray::Bind() const {
	PTGN_CHECK(IsValid(), "Cannot bind uninitialized or destroyed vertex array");
	glBindVertexArray(instance_->id_);
}

void VertexArray::Unbind() const {
	glBindVertexArray(0);
}

void VertexArray::Draw() const {
	PTGN_CHECK(IsValid(), "Cannot draw uninitialized or destroyed vertex array");
	Bind();
	const IndexBuffer& ibo{ GetIndexBuffer() };
	if (ibo.IsValid()) {
		glDrawElements(
			static_cast<GLenum>(GetPrimitiveMode()),
			ibo.GetCount(),
			static_cast<GLenum>(ibo.GetType()),
			nullptr
		);
	} else {
		glDrawArrays(
			static_cast<GLenum>(GetPrimitiveMode()),
			0,
			GetVertexBuffer().GetCount()
		);
	}
	Unbind();
}

void VertexArray::SetVertexBuffer(const VertexBuffer& vertex_buffer) {
	PTGN_CHECK(IsValid(), "Cannot add vertex buffer to uninitialized or destroyed vertex array");
	const BufferLayout& layout = vertex_buffer.GetLayout();
	PTGN_ASSERT(!layout.IsEmpty(), "Cannot add a vertex buffer with an empty (unset) layout to a vertex array");

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
	instance_->vertex_buffer_ = vertex_buffer;
}

void VertexArray::SetIndexBuffer(const IndexBuffer& index_buffer) {
	PTGN_CHECK(IsValid(), "Cannot set vertex buffer of uninitialized or destroyed vertex array");
	glBindVertexArray(instance_->id_);
	index_buffer.Bind();
	instance_->index_buffer_ = index_buffer;
}

const VertexBuffer& VertexArray::GetVertexBuffer() const {
	PTGN_CHECK(IsValid(), "Cannot get vertex buffer of uninitialized or destroyed vertex array");
	PTGN_CHECK(instance_->vertex_buffer_.IsValid(), "Cannot get vertex buffer which is uninitialized or destroyed");
	return instance_->vertex_buffer_;
}

const IndexBuffer& VertexArray::GetIndexBuffer() const {
	PTGN_CHECK(IsValid(), "Cannot get index buffer of uninitialized or destroyed vertex array");
	PTGN_CHECK(instance_->index_buffer_.IsValid(), "Cannot get index buffer which is uninitialized or destroyed");
	return instance_->index_buffer_;
}


void VertexArray::SetPrimitiveMode(PrimitiveMode mode) {
	PTGN_CHECK(IsValid(), "Cannot set primitive mode of uninitialized or destroyed vertex array");
	instance_->mode_ = mode;
}

PrimitiveMode VertexArray::GetPrimitiveMode() const {
	PTGN_CHECK(IsValid(), "Cannot get primitive mode of uninitialized or destroyed vertex array");
	return instance_->mode_;
}

} // namespace ptgn