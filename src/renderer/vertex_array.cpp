#include "vertex_array.h"

#include <cassert>

#include "gl_loader.h"

namespace ptgn {

std::shared_ptr<VertexArray> ptgn::VertexArray::Create() {
	return std::make_shared<VertexArray>();
}

VertexArray::VertexArray() {
	glGenVertexArrays(1, &id_);
}

VertexArray::~VertexArray() {
	glDeleteVertexArrays(1, &id_);
}

void VertexArray::Bind() const {
	glBindVertexArray(id_);
}

void VertexArray::Unbind() const {
	glBindVertexArray(0);
}

void VertexArray::AddVertexBuffer(const std::shared_ptr<AbstractVertexBuffer>& vertex_buffer) {
	const BufferLayout& layout = vertex_buffer->GetLayout();
	assert(!layout.IsEmpty() && "Cannot add a vertex buffer with an empty (unset) layout to a vertex array");

	glBindVertexArray(id_);
	vertex_buffer->Bind();
	

	const std::vector<BufferElement>& elements = layout.GetElements();
	for (std::size_t i = 0; i < elements.size(); ++i) {
		const BufferElement& element{ elements[i] };
		ShaderDataInfo info{ element.GetDataType() };
		glEnableVertexAttribArray(i);
		glVertexAttribPointer(
			i, info.count,
			static_cast<GLenum>(vertex_buffer->GetType()),
			element.IsNormalized() ? GL_TRUE : GL_FALSE,
			layout.GetStride(),
			(const void*)element.GetOffset()
		);
		// Not required according to: https://stackoverflow.com/a/12428035
		//glDisableVertexAttribArray(i);
	}
	vertex_buffers_.push_back(vertex_buffer);
}

void VertexArray::SetIndexBuffer(const std::shared_ptr<IndexBuffer>& index_buffer) {
	glBindVertexArray(id_);
	index_buffer->Bind();
	index_buffer_ = index_buffer;
}

VertexArray::Id VertexArray::GetId() const {
	return id_;
}

} // namespace ptgn