#include "protegon/vertex_array.h"

#include "protegon/debug.h"
#include "protegon/renderer.h"
#include "renderer/gl_loader.h"

namespace ptgn {

namespace impl {

VertexArrayInstance::~VertexArrayInstance() {
	gl::DeleteVertexArrays(1, &id_);
}

VertexArrayInstance::VertexArrayInstance(PrimitiveMode mode) : mode_{ mode } {
	gl::GenVertexArrays(1, &id_);
}

} // namespace impl

VertexArray::VertexArray(
	PrimitiveMode mode, const VertexBuffer& vertex_buffer, const IndexBuffer& index_buffer
) {
	instance_ = std::shared_ptr<impl::VertexArrayInstance>(new impl::VertexArrayInstance(mode));
	if (vertex_buffer.IsValid()) {
		SetVertexBuffer(vertex_buffer);
	}
	if (index_buffer.IsValid()) {
		SetIndexBuffer(index_buffer);
	}
}

VertexArray::VertexArray(const std::shared_ptr<impl::VertexArrayInstance>& instance) :
	Handle<impl::VertexArrayInstance>{ instance } {}

void VertexArray::Bind() const {
	PTGN_CHECK(IsValid(), "Cannot bind uninitialized or destroyed vertex array");
	gl::BindVertexArray(instance_->id_);
}

void VertexArray::Unbind() const {
	gl::BindVertexArray(0);
}

void VertexArray::SetVertexBuffer(const VertexBuffer& vertex_buffer) {
	PTGN_CHECK(IsValid(), "Cannot add vertex buffer to uninitialized or destroyed vertex array");
	const impl::BufferLayout& layout = vertex_buffer.GetLayout();
	PTGN_ASSERT(
		!layout.IsEmpty(),
		"Cannot add a vertex buffer with an empty (unset) layout to a vertex array"
	);

	gl::BindVertexArray(instance_->id_);
	vertex_buffer.Bind();

	const std::vector<impl::BufferElement>& elements = layout.GetElements();
	for (std::uint32_t i = 0; i < elements.size(); ++i) {
		const impl::BufferElement& element{ elements[i] };
		gl::EnableVertexAttribArray(i);
		gl::VertexAttribPointer(
			i, element.GetCount(), static_cast<gl::GLenum>(element.GetType()),
			element.IsNormalized() ? GL_TRUE : GL_FALSE, layout.GetStride(),
			(const void*)element.GetOffset()
		);
		// Not required according to: https://stackoverflow.com/a/12428035
		// glDisableVertexAttribArray(i);
	}

	vertex_buffer.Unbind();

	instance_->vertex_buffer_ = vertex_buffer;
}

void VertexArray::SetIndexBuffer(const IndexBuffer& index_buffer) {
	PTGN_CHECK(IsValid(), "Cannot set vertex buffer of uninitialized or destroyed vertex array");
	gl::BindVertexArray(instance_->id_);
	index_buffer.Bind();
	instance_->index_buffer_ = index_buffer;
}

const VertexBuffer& VertexArray::GetVertexBuffer() const {
	PTGN_CHECK(IsValid(), "Cannot get vertex buffer of uninitialized or destroyed vertex array");
	return instance_->vertex_buffer_;
}

const IndexBuffer& VertexArray::GetIndexBuffer() const {
	PTGN_CHECK(IsValid(), "Cannot get index buffer of uninitialized or destroyed vertex array");
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