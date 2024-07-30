#pragma once

#include <array>
#include <cstdint>
#include <initializer_list>
#include <vector>

#include "renderer/gl_helper.h"

namespace ptgn {

namespace impl {

class InternalBufferLayout;

struct BufferElement {
	BufferElement(std::uint16_t size, std::uint16_t count, impl::GLType type) :
		size{ size }, count{ count }, type{ type } {}

	std::uint16_t size{ 0 };  // Number of elements x Size of element.
	std::uint16_t count{ 0 }; // Number of elements
	impl::GLType type{ 0 };	  // Type of buffer element (i.e. GL_FLOAT)
	// Set by BufferLayout.
	std::size_t offset{ 0 }; // Number of bytes from start of buffer.
	// Whether or not the buffer elements are normalized. See here for more info:
	// https://registry.khronos.org/OpenGL-Refpages/es3.0/html/glVertexAttribPointer.xhtml
	bool normalized{ false };
};

} // namespace impl

template <typename... BufferElements>
class BufferLayout {
public:
	BufferLayout() {
		elements_.reserve(sizeof...(BufferElements));
		elements_ = {
			impl::BufferElement{static_cast<std::uint16_t>(sizeof(BufferElements)),
								 static_cast<std::uint16_t>(std::tuple_size<BufferElements>::value),
								 impl::GetType<typename BufferElements::value_type>()}
			   ...
		};
	}

private:
	friend class impl::InternalBufferLayout;

	const std::vector<impl::BufferElement>& GetElements() const {
		return elements_;
	}

	std::vector<impl::BufferElement> elements_;
};

namespace impl {

class InternalBufferLayout {
public:
	InternalBufferLayout() = default;

	template <typename... BufferElements>
	InternalBufferLayout(const BufferLayout<BufferElements...>& layout) :
		elements_{ layout.GetElements() } {
		CalculateOffsets();
	}

	[[nodiscard]] const std::vector<BufferElement>& GetElements() const;
	[[nodiscard]] std::int32_t GetStride() const;
	[[nodiscard]] bool IsEmpty() const;

private:
	void CalculateOffsets();

	std::vector<BufferElement> elements_;
	std::int32_t stride_{ 0 };
};

} // namespace impl

} // namespace ptgn