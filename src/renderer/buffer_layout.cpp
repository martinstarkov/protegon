#include "renderer/buffer_layout.h"

namespace ptgn {

namespace impl {

const std::vector<BufferElement>& InternalBufferLayout::GetElements() const {
	return elements_;
}

std::int32_t InternalBufferLayout::GetStride() const {
	return stride_;
}

bool InternalBufferLayout::IsEmpty() const {
	return elements_.size() == 0;
}

void InternalBufferLayout::CalculateOffsets() {
	std::int32_t offset = 0;
	stride_				= 0;
	for (BufferElement& element : elements_) {
		element.offset	= offset;
		offset		   += element.size;
	}
	stride_ = offset;
}

} // namespace impl

} // namespace ptgn