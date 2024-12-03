#pragma once

#include <cstdint>
#include <type_traits>
#include <utility>
#include <vector>

#include "renderer/gl_helper.h"

namespace ptgn {

class VertexArray;

namespace impl {

struct BufferElement {
	constexpr BufferElement(
		std::uint16_t size, std::uint16_t count, impl::GLType type, bool is_integer
	) :
		size{ size }, count{ count }, type{ type }, is_integer{ is_integer } {}

	std::uint16_t size{ 0 };  // Number of elements x Size of element.
	std::uint16_t count{ 0 }; // Number of elements
	impl::GLType type{ 0 };	  // Type of buffer element (i.e. GL_FLOAT)
	// Set by BufferLayout.
	std::size_t offset{ 0 }; // Number of bytes from start of buffer.
	// Whether or not the buffer elements are normalized. See here for more info:
	// https://registry.khronos.org/OpenGL-Refpages/es3.0/html/glVertexAttribPointer.xhtml
	bool is_integer{ false };
	bool normalized{ false };
};

} // namespace impl

template <typename... Ts>
class BufferLayout {
	static_assert(
		(impl::is_vertex_data_type<Ts> && ...),
		"Provided vertex type should only contain ptgn::glsl:: types"
	);
	static_assert(sizeof...(Ts) > 0, "Must provide layout types as template arguments");

public:
	constexpr BufferLayout() {
		CalculateOffsets();
	}

	[[nodiscard]] constexpr std::int32_t GetStride() const {
		return stride_;
	}

	[[nodiscard]] constexpr bool IsEmpty() const {
		return elements_.empty();
	}

private:
	friend class VertexArray;

	template <typename T>
	[[nodiscard]] constexpr static bool IsInteger() {
		static_assert(
			impl::is_vertex_data_type<T>,
			"Provided vertex type should only contain ptgn::glsl:: types"
		);
		using V = typename T::value_type;
		return std::is_same_v<V, bool> || std::is_same_v<V, std::int32_t> ||
			   std::is_same_v<V, std::uint32_t>;
	}

	std::int32_t stride_{ 0 };

	std::array<impl::BufferElement, sizeof...(Ts)> elements_{ impl::BufferElement{
		static_cast<std::uint16_t>(sizeof(Ts)),
		static_cast<std::uint16_t>(std::tuple_size<Ts>::value),
		impl::GetType<typename Ts::value_type>(), IsInteger<Ts>() }... };

	constexpr void CalculateOffsets() {
		std::int32_t offset{ 0 };
		stride_ = 0;
		for (impl::BufferElement& element : elements_) {
			element.offset	= offset;
			offset		   += element.size;
		}
		stride_ = offset;
	}

public:
	[[nodiscard]] constexpr const std::array<impl::BufferElement, sizeof...(Ts)>& GetElements(
	) const {
		return elements_;
	}
};

} // namespace ptgn