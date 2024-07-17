#include "protegon/buffer_layout.h"

namespace ptgn {

namespace impl {

BufferElement::BufferElement(
	std::uint16_t size_of_element, std::uint16_t count, impl::GLType type, bool normalized
) :
	size_{ static_cast<std::uint16_t>(size_of_element * count) },
	count_{ count },
	type_{ type },
	normalized_{ normalized } {}

std::uint16_t BufferElement::GetSize() const {
	return size_;
}

std::uint16_t BufferElement::GetCount() const {
	return count_;
}

impl::GLType BufferElement::GetType() const {
	return type_;
}

std::size_t BufferElement::GetOffset() const {
	return offset_;
}

bool BufferElement::IsNormalized() const {
	return normalized_;
}

BufferLayout::BufferLayout(const std::initializer_list<BufferElement>& elements) :
	elements_{ elements } {
	CalculateOffsets();
}

bool BufferLayout::IsEmpty() const {
	return elements_.size() == 0;
}

const std::vector<BufferElement>& BufferLayout::GetElements() const {
	return elements_;
}

std::int32_t BufferLayout::GetStride() const {
	return stride_;
}

void BufferLayout::CalculateOffsets() {
	std::int32_t offset = 0;
	stride_				= 0;
	for (BufferElement& element : elements_) {
		element.offset_	 = offset;
		offset			+= element.size_;
	}
	stride_ = offset;
}

} // namespace impl

} // namespace ptgn