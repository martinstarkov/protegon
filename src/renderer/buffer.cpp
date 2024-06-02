#include "buffer.h"

#include "gl_loader.h"
#include <cassert>

namespace ptgn {

namespace impl {

template <> inline constexpr GLEnumType GetTypeIdentifier<std::int8_t>()   { return GL_BYTE;		   }
template <> inline constexpr GLEnumType GetTypeIdentifier<std::uint8_t>()  { return GL_UNSIGNED_BYTE;  }
template <> inline constexpr GLEnumType GetTypeIdentifier<std::int16_t>()  { return GL_SHORT;		   }
template <> inline constexpr GLEnumType GetTypeIdentifier<std::uint16_t>() { return GL_UNSIGNED_SHORT; }
template <> inline constexpr GLEnumType GetTypeIdentifier<std::int32_t>()  { return GL_INT;			   }
template <> inline constexpr GLEnumType GetTypeIdentifier<std::uint32_t>() { return GL_UNSIGNED_INT;   }
template <> inline constexpr GLEnumType GetTypeIdentifier<std::float_t>()  { return GL_FLOAT;		   }
template <> inline constexpr GLEnumType GetTypeIdentifier<std::double_t>() { return GL_DOUBLE;		   }

} // namespace impl

template <typename T>
VertexBuffer<T>::VertexBuffer(const std::vector<T>& vertices) : vertices_{ vertices } {
	glGenBuffers(1, &id_);
	glBindBuffer(GL_ARRAY_BUFFER, id_);
	glBufferData(GL_ARRAY_BUFFER, sizeof(T) * vertices_.size(), vertices_.data(), GL_STATIC_DRAW);
}

template <typename T>
VertexBuffer<T>::~VertexBuffer() {
	glDeleteBuffers(1, &id_);
}

template <typename T>
void VertexBuffer<T>::SetLayout(const BufferLayout& layout) {
	layout_ = layout;
	const std::vector<BufferElement>& elements = layout_.GetElements();
	for (std::size_t i = 0; i < elements.size(); ++i) {
		const BufferElement& element{ elements[i] };
		ShaderDataInfo info{ element.GetDataType()};
		glEnableVertexAttribArray(i);
		glVertexAttribPointer(
			i, 
			info.count, 
			static_cast<GLenum>(impl::GetTypeIdentifier<T>()), 
			element.IsNormalized() ? GL_TRUE : GL_FALSE,
			layout.GetStride(),
			(const void*)element.GetOffset()
		);
	}
}

template <typename T>
const BufferLayout& VertexBuffer<T>::GetLayout() const {
	return layout_;
}

template <typename T>
void VertexBuffer<T>::Bind() const {
	glBindBuffer(GL_ARRAY_BUFFER, id_);
}

template <typename T>
void VertexBuffer<T>::Unbind() const {
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

template <typename T>
IndexBuffer::Id VertexBuffer<T>::GetId() const {
	return id_;
}

template <typename T>
std::shared_ptr<VertexBuffer<T>> VertexBuffer<T>::Create(const std::vector<T>& vertices) {
	return std::shared_ptr<VertexBuffer>(new VertexBuffer(vertices));
}

IndexBuffer::IndexBuffer(const std::vector<Type>& indices) : indices_{ indices } {
	glGenBuffers(1, &id_);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id_);
	std::size_t type_size = sizeof(std::remove_reference_t<decltype(indices_)>::value_type);
	std::size_t size = type_size * indices_.size();
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, indices_.data(), GL_STATIC_DRAW);
}

IndexBuffer::~IndexBuffer() {
	glDeleteBuffers(1, &id_);
}

void IndexBuffer::Bind() const {
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id_);
}

void IndexBuffer::Unbind() const {
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

IndexBuffer::Id IndexBuffer::GetId() const {
	return id_;
}

std::shared_ptr<IndexBuffer> IndexBuffer::Create(const std::vector<Type>& indices) {
	return std::shared_ptr<IndexBuffer>(new IndexBuffer(indices));
}

std::size_t IndexBuffer::GetCount() const {
	return indices_.size();
}

BufferElement::BufferElement(ShaderDataType data_type, bool normalized) : data_type_{ data_type }, normalized_{ normalized } {}

ShaderDataType BufferElement::GetDataType() const {
	return data_type_;
}

bool BufferElement::IsNormalized() const {
	return normalized_;
}

std::size_t BufferElement::GetSize() const {
	return size_;
}

std::size_t BufferElement::GetOffset() const {
	return offset_;
}

void BufferElement::SetSize(std::size_t size) {
	size_ = size;
}

void BufferElement::SetOffset(std::size_t offset) {
	offset_ = offset;
}

BufferLayout::BufferLayout(const std::initializer_list<BufferElement>& elements) : elements_{ elements } {
	CalculateOffsets();
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
		element.SetOffset(offset);
		ShaderDataInfo info{ element.GetDataType() };
		element.SetSize(info.size * info.count);
		offset += element.GetSize();
	}
	stride_ = offset;
}

// Explicit template instantiation
template class VertexBuffer<std::int8_t>;
template class VertexBuffer<std::uint8_t>;
template class VertexBuffer<std::int16_t>;
template class VertexBuffer<std::uint16_t>;
template class VertexBuffer<std::int32_t>;
template class VertexBuffer<std::uint32_t>;
template class VertexBuffer<std::float_t>;
template class VertexBuffer<std::double_t>;

} // namespace ptgn