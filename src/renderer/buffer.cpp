#include "buffer.h"

#include "utility/debug.h"

#include "gl_loader.h"

namespace ptgn {

BufferElement::BufferElement(std::uint16_t size_of_element, std::uint16_t count, impl::GLSLType type, bool normalized) :
	size_{ static_cast<std::uint16_t>(size_of_element * count) }, count_{ count }, type_{ type }, normalized_{ normalized } {}

//BufferElement::BufferElement(ShaderDataType data_type, bool normalized) : normalized_{ normalized } {
//	ShaderDataInfo info{ data_type };
//	// ShaderDataInfo size stores the size of a single buffer element.
//	size_ = info.size * info.count;
//	count_ = info.count;
//	type_ = info.type;
//}

std::uint16_t BufferElement::GetSize() const {
	return size_;
}

std::uint16_t BufferElement::GetCount() const {
	return count_;
}

impl::GLSLType BufferElement::GetType() const {
	return type_;
}

std::size_t BufferElement::GetOffset() const {
	return offset_;
}

bool BufferElement::IsNormalized() const {
	return normalized_;
}

BufferLayout::BufferLayout(const std::initializer_list<BufferElement>& elements) : elements_{ elements } {
	CalculateOffsets();
}

BufferLayout::BufferLayout(const std::vector<BufferElement>& elements) : elements_{ elements } {
	CalculateOffsets();
}

bool BufferLayout::IsEmpty() const {
	return elements_.size() == 0;
}

const std::vector<BufferElement>& BufferLayout::GetElements() const {
	return elements_;
}

std::size_t BufferLayout::GetStride() const {
	return stride_;
}

void BufferLayout::CalculateOffsets() {
	std::size_t offset = 0;
	stride_ = 0;
	for (BufferElement& element : elements_) {
		element.offset_ = offset;
		offset += element.size_;
	}
	stride_ = offset;
}

namespace impl {

VertexBufferInstance::~VertexBufferInstance() {
	glDeleteBuffers(1, &id_);
}

void VertexBufferInstance::GenerateBuffer(void* vertex_data, std::size_t size) {
	glGenBuffers(1, &id_);
	glBindBuffer(GL_ARRAY_BUFFER, id_);
	glBufferData(GL_ARRAY_BUFFER, size, vertex_data, GL_STATIC_DRAW);
}

IndexBufferInstance::IndexBufferInstance(const std::vector<std::uint32_t>& indices) : count_{ indices.size() } {
	PTGN_ASSERT(indices.size() > 0);
	using IndexType = std::remove_reference_t<decltype(indices)>::value_type;
	GenerateBuffer((void*)indices.data(), sizeof(std::uint32_t) * count_);
}

void IndexBufferInstance::GenerateBuffer(void* index_data, std::size_t size) {
	glGenBuffers(1, &id_);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id_);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, index_data, GL_STATIC_DRAW);
}

IndexBufferInstance::~IndexBufferInstance() {
	glDeleteBuffers(1, &id_);
}

} // namespace impl

const BufferLayout& VertexBuffer::GetLayout() const {
	PTGN_CHECK(IsValid(), "Cannot get layout of uninitialized or destroyed vertex buffer");
	return instance_->layout_;
}

void VertexBuffer::Bind() const {
	PTGN_CHECK(IsValid(), "Cannot bind uninitialized or destroyed vertex buffer");
	glBindBuffer(GL_ARRAY_BUFFER, instance_->id_);
}

void VertexBuffer::Unbind() const {
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

std::size_t VertexBuffer::GetCount() const {
	PTGN_CHECK(IsValid(), "Cannot get count of uninitialized or destroyed vertex buffer");
	return instance_->count_;
}

IndexBuffer::IndexBuffer(const std::vector<std::uint32_t>& indices) {
	instance_ = std::shared_ptr<impl::IndexBufferInstance>(new impl::IndexBufferInstance(indices));
}

void IndexBuffer::Bind() const {
	PTGN_CHECK(IsValid(), "Cannot bind uninitialized or destroyed index buffer");
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, instance_->id_);
}

void IndexBuffer::Unbind() const {
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

std::size_t IndexBuffer::GetCount() const {
	PTGN_CHECK(IsValid(), "Cannot get count of uninitialized or destroyed index buffer");
	return instance_->count_;
}

} // namespace ptgn