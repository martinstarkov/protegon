#pragma once

#include <array>
#include <cstdint>
#include <initializer_list>
#include <vector>

#include "renderer/gl_helper.h"

namespace ptgn {

namespace impl {

class BufferLayout;

class BufferElement {
public:
	BufferElement() = default;
	BufferElement(
		std::uint16_t size_of_element, std::uint16_t count, impl::GLType type,
		bool normalized = false
	);
	// BufferElement(ShaderDataType data_type, bool normalized = false);
	[[nodiscard]] std::uint16_t GetSize() const;
	[[nodiscard]] std::uint16_t GetCount() const;
	[[nodiscard]] impl::GLType GetType() const;
	[[nodiscard]] bool IsNormalized() const;
	[[nodiscard]] std::size_t GetOffset() const;

private:
	friend class BufferLayout;
	std::uint16_t size_{ 0 };  // Number of elements x Size of element.
	std::uint16_t count_{ 0 }; // Number of elements
	impl::GLType type_{ 0 }; // Type of buffer element (i.e. GL_FLOAT)
	// Set by BufferLayout.
	std::size_t offset_{ 0 }; // Number of bytes from start of buffer.
	// Whether or not the buffer elements are normalized. See here for more info:
	// https://registry.khronos.org/OpenGL-Refpages/es3.0/html/glVertexAttribPointer.xhtml
	bool normalized_{ false };
};

class BufferLayout {
public:
	BufferLayout() = default;
	BufferLayout(const std::initializer_list<BufferElement>& elements);

	template <std::size_t I>
	constexpr BufferLayout(const std::array<BufferElement, I>& elements) :
		elements_{ std::begin(elements), std::end(elements) } {
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