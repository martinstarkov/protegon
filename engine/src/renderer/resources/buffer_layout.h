#pragma once

#include <array>
#include <cstdint>
#include <tuple>
#include <type_traits>

#include "core/util/concepts.h"
#include "renderer/resources/glsl_types.h"

namespace ptgn::impl::gl {

template <typename T>
concept VertexDataType = IsAnyOf<
	T, glsl::float_, glsl::vec2, glsl::vec3, glsl::vec4, glsl::double_, glsl::dvec2, glsl::dvec3,
	glsl::dvec4, glsl::bool_, glsl::bvec2, glsl::bvec3, glsl::bvec4, glsl::int_, glsl::ivec2,
	glsl::ivec3, glsl::ivec4, glsl::uint_, glsl::uvec2, glsl::uvec3, glsl::uvec4>;

enum class BufferElementType : std::uint8_t {
	Float,
	Double,
	Int,
	UInt,
	Short,
	UShort,
	Byte,
	UByte,
	Bool
};

template <typename T>
struct BufferType {
	static_assert(sizeof(T) == 0, "Unsupported buffer element type");
};

template <>
struct BufferType<float> {
	static constexpr auto value = BufferElementType::Float;
};

template <>
struct BufferType<double> {
	static constexpr auto value = BufferElementType::Double;
};

template <>
struct BufferType<std::int32_t> {
	static constexpr auto value = BufferElementType::Int;
};

template <>
struct BufferType<std::uint32_t> {
	static constexpr auto value = BufferElementType::UInt;
};

template <>
struct BufferType<std::int16_t> {
	static constexpr auto value = BufferElementType::Short;
};

template <>
struct BufferType<std::uint16_t> {
	static constexpr auto value = BufferElementType::UShort;
};

template <>
struct BufferType<std::int8_t> {
	static constexpr auto value = BufferElementType::Byte;
};

template <>
struct BufferType<std::uint8_t> {
	static constexpr auto value = BufferElementType::UByte;
};

template <>
struct BufferType<bool> {
	static constexpr auto value = BufferElementType::Bool;
};

template <typename T>
constexpr BufferElementType GetBufferElementType() {
	return BufferType<typename T::value_type>::value;
}

struct BufferElement {
	constexpr BufferElement(
		std::uint16_t buffer_size, std::uint16_t buffer_count, bool buffer_is_integer,
		BufferElementType type
	) :
		size{ buffer_size }, count{ buffer_count }, is_integer{ buffer_is_integer }, type{ type } {}

	std::uint16_t size{ 0 };  // Number of elements x Size of element.
	std::uint16_t count{ 0 }; // Number of elements
	BufferElementType type{ BufferElementType::Float };
	// Set by BufferLayout.
	std::size_t offset{ 0 }; // Number of bytes from start of buffer.
	// Whether or not the buffer elements are normalized. See here for more info:
	// https://registry.khronos.org/OpenGL-Refpages/es3.0/html/glVertexAttribPointer.xhtml
	bool is_integer{ false };
	bool normalized{ false };
};

template <VertexDataType... Ts>
	requires NonEmptyPack<Ts...>
struct BufferLayout {
	constexpr BufferLayout() {
		CalculateOffsets();
	}

	[[nodiscard]] constexpr std::int32_t GetStride() const {
		return stride_;
	}

	[[nodiscard]] constexpr bool IsEmpty() const {
		return elements_.empty();
	}

	template <VertexDataType T>
	[[nodiscard]] constexpr static bool IsInteger() {
		using V = typename T::value_type;
		return std::is_same_v<V, bool> || std::is_same_v<V, std::int32_t> ||
			   std::is_same_v<V, std::uint32_t>;
	}

	std::int32_t stride_{ 0 };

	std::array<BufferElement, sizeof...(Ts)> elements_{ BufferElement{
		static_cast<std::uint16_t>(sizeof(Ts)),
		static_cast<std::uint16_t>(std::tuple_size<Ts>::value),
		IsInteger<Ts>(),
		GetBufferElementType<Ts>(),
	}... };

	constexpr void CalculateOffsets() {
		std::size_t offset{ 0 };
		stride_ = 0;
		for (BufferElement& element : elements_) {
			element.offset	= offset;
			offset		   += element.size;
		}
		stride_ = static_cast<std::int32_t>(offset);
	}

	[[nodiscard]] constexpr const std::array<BufferElement, sizeof...(Ts)>& GetElements() const {
		return elements_;
	}
};

template <typename Derived, typename... Elements>
struct VertexLayout {
	static constexpr BufferLayout<Elements...> GetLayout() {
		return {};
	}
};

} // namespace ptgn::impl::gl