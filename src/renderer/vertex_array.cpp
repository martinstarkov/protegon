#include "protegon/vertex_array.h"

#include "renderer/gl_loader.h"
#include "utility/debug.h"

namespace ptgn {

namespace impl {

VertexArrayInstance::VertexArrayInstance() {
	gl::GenVertexArrays(1, &id_);
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
	if (vertex_buffer.IsValid()) {
		SetVertexBuffer(vertex_buffer);
	}
	if (index_buffer.IsValid()) {
		SetIndexBuffer(index_buffer);
	}
}

void VertexArray::Bind() const {
	PTGN_CHECK(IsValid(), "Cannot bind uninitialized or destroyed vertex array");
	gl::BindVertexArray(instance_->id_);
}

void VertexArray::Unbind() const {
	gl::BindVertexArray(0);
}

void VertexArray::SetVertexBuffer(const VertexBuffer& vertex_buffer) {
	if (!IsValid()) {
		instance_ = std::make_shared<impl::VertexArrayInstance>();
	}
	PTGN_CHECK(IsValid(), "Cannot add vertex buffer to uninitialized or destroyed vertex array");
	PTGN_CHECK(vertex_buffer.IsValid());
	PTGN_ASSERT(
		!vertex_buffer.instance_->layout_.IsEmpty(),
		"Cannot add a vertex buffer with an empty (unset) layout to a vertex array"
	);

	Bind();
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
			i, element.GetCount(), static_cast<gl::GLenum>(element.GetType()),
			element.IsNormalized() ? GL_TRUE : GL_FALSE,
			vertex_buffer.instance_->layout_.GetStride(), (const void*)element.GetOffset()
		);
		// Not required according to: https://stackoverflow.com/a/12428035
		// glDisableVertexAttribArray(i);
	}

	vertex_buffer.Unbind();

	instance_->vertex_buffer_ = vertex_buffer;
}

void VertexArray::SetIndexBuffer(const IndexBuffer& index_buffer) {
	if (!IsValid()) {
		instance_ = std::make_shared<impl::VertexArrayInstance>();
	}
	PTGN_CHECK(IsValid(), "Cannot set index buffer of uninitialized or destroyed vertex array");
	Bind();
	index_buffer.Bind();
	instance_->index_buffer_ = index_buffer;
}

void VertexArray::SetPrimitiveMode(PrimitiveMode mode) {
	if (!IsValid()) {
		instance_ = std::make_shared<impl::VertexArrayInstance>();
	}
	PTGN_CHECK(IsValid(), "Cannot set primitive mode of uninitialized or destroyed vertex array");
	instance_->mode_ = mode;
}

const VertexBuffer& VertexArray::GetVertexBuffer() const {
	PTGN_CHECK(IsValid(), "Cannot get vertex buffer of uninitialized or destroyed vertex array");
	return instance_->vertex_buffer_;
}

const IndexBuffer& VertexArray::GetIndexBuffer() const {
	PTGN_CHECK(IsValid(), "Cannot get index buffer of uninitialized or destroyed vertex array");
	return instance_->index_buffer_;
}

PrimitiveMode VertexArray::GetPrimitiveMode() const {
	PTGN_CHECK(IsValid(), "Cannot get primitive mode of uninitialized or destroyed vertex array");
	return instance_->mode_;
}

} // namespace ptgn