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
	BufferElement(std::uint16_t size, std::uint16_t count, impl::GLType type, bool is_integer) :
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
	BufferLayout() {
		elements_.reserve(sizeof...(Ts));
		elements_ = { impl::BufferElement{ static_cast<std::uint16_t>(sizeof(Ts)),
										   static_cast<std::uint16_t>(std::tuple_size<Ts>::value),
										   impl::GetType<typename Ts::value_type>(),
										   IsInteger<Ts>() }... };
	}

private:
	friend class impl::InternalBufferLayout;

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