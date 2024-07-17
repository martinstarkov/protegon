#include "protegon/buffer.h"

#include "renderer/gl_loader.h"

namespace ptgn {

namespace impl {

VertexBufferInstance::~VertexBufferInstance() {
	gl::DeleteBuffers(1, &id_);
}

void VertexBufferInstance::GenerateBuffer(void* vertex_data, std::size_t size) {
	gl::GenBuffers(1, &id_);
	gl::BindBuffer(GL_ARRAY_BUFFER, id_);
	gl::BufferData(GL_ARRAY_BUFFER, size, vertex_data, GL_STATIC_DRAW);
}

void VertexBufferInstance::GenerateBuffer(std::size_t size) {
	gl::GenBuffers(1, &id_);
	gl::BindBuffer(GL_ARRAY_BUFFER, id_);
	gl::BufferData(GL_ARRAY_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
}

void VertexBufferInstance::SetBuffer(void* vertex_data, std::size_t size) {
	gl::BindBuffer(GL_ARRAY_BUFFER, id_);
	gl::BufferSubData(GL_ARRAY_BUFFER, 0, size, vertex_data);
}

IndexBufferInstance::IndexBufferInstance(const std::vector<std::uint32_t>& indices) :
	count_{ static_cast<std::int32_t>(indices.size()) } {
	PTGN_ASSERT(indices.size() > 0);
	using IndexType = std::remove_reference_t<decltype(indices)>::value_type;
	GenerateBuffer((void*)indices.data(), sizeof(std::uint32_t) * count_);
}

void IndexBufferInstance::GenerateBuffer(void* index_data, std::size_t size) {
	gl::GenBuffers(1, &id_);
	gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, id_);
	gl::BufferData(GL_ELEMENT_ARRAY_BUFFER, size, index_data, GL_STATIC_DRAW);
}

IndexBufferInstance::~IndexBufferInstance() {
	gl::DeleteBuffers(1, &id_);
}

std::uint32_t IndexBufferInstance::GetType() {
	return static_cast<std::uint32_t>(impl::GLType::UnsignedInt);
}

} // namespace impl

void VertexBuffer::Bind() const {
	PTGN_CHECK(IsValid(), "Cannot bind uninitialized or destroyed vertex buffer");
	gl::BindBuffer(GL_ARRAY_BUFFER, instance_->id_);
}

void VertexBuffer::Unbind() const {
	gl::BindBuffer(GL_ARRAY_BUFFER, 0);
}

std::int32_t VertexBuffer::GetCount() const {
	PTGN_CHECK(IsValid(), "Cannot get count of uninitialized or destroyed vertex buffer");
	return instance_->count_;
}

const impl::BufferLayout& VertexBuffer::GetLayout() const {
	PTGN_CHECK(IsValid(), "Cannot get layout of uninitialized or destroyed vertex buffer");
	return instance_->layout_;
}

IndexBuffer::IndexBuffer(const std::initializer_list<std::uint32_t>& indices) {
	instance_ = std::shared_ptr<impl::IndexBufferInstance>(new impl::IndexBufferInstance(indices));
}

void IndexBuffer::Bind() const {
	PTGN_CHECK(IsValid(), "Cannot bind uninitialized or destroyed index buffer");
	gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, instance_->id_);
}

void IndexBuffer::Unbind() const {
	gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

std::int32_t IndexBuffer::GetCount() const {
	PTGN_CHECK(IsValid(), "Cannot get count of uninitialized or destroyed index buffer");
	return instance_->count_;
}

} // namespace ptgn