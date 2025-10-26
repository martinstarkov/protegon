#pragma once

#include <array>
#include <cstdint>
#include <type_traits>

#include "core/util/concepts.h"

namespace ptgn::impl {

// Vertex Types

namespace glsl {

using float_ = std::array<float, 1>;
using vec2	 = std::array<float, 2>;
using vec3	 = std::array<float, 3>;
using vec4	 = std::array<float, 4>;

using double_ = std::array<double, 1>;
using dvec2	  = std::array<double, 2>;
using dvec3	  = std::array<double, 3>;
using dvec4	  = std::array<double, 4>;

using bool_ = std::array<bool, 1>;
using bvec2 = std::array<bool, 2>;
using bvec3 = std::array<bool, 3>;
using bvec4 = std::array<bool, 4>;

using int_	= std::array<std::int32_t, 1>;
using ivec2 = std::array<std::int32_t, 2>;
using ivec3 = std::array<std::int32_t, 3>;
using ivec4 = std::array<std::int32_t, 4>;

using uint_ = std::array<std::uint32_t, 1>;
using uvec2 = std::array<std::uint32_t, 2>;
using uvec3 = std::array<std::uint32_t, 3>;
using uvec4 = std::array<std::uint32_t, 4>;

} // namespace glsl

enum class PrimitiveMode : std::uint32_t {
	Points		  = 0x0000, // GL_POINTS
	Lines		  = 0x0001, // GL_LINES
	LineLoop	  = 0x0002, // GL_LINE_LOOP
	LineStrip	  = 0x0003, // GL_LINE_STRIP
	Triangles	  = 0x0004, // GL_TRIANGLES
	TriangleStrip = 0x0005, // GL_TRIANGLE_STRIP
	TriangleFan	  = 0x0006, // GL_TRIANGLE_FAN
};

enum class BufferUsage {
	Unset		= -1,
	StreamDraw	= 0x88E0, // GL_STREAM_DRAW
	StreamRead	= 0x88E1, // GL_STREAM_READ
	StreamCopy	= 0x88E2, // GL_STREAM_COPY
	StaticDraw	= 0x88E4, // GL_STATIC_DRAW
	StaticRead	= 0x88E5, // GL_STATIC_READ
	StaticCopy	= 0x88E6, // GL_STATIC_COPY
	DynamicDraw = 0x88E8, // GL_DYNAMIC_DRAW
	DynamicRead = 0x88E9, // GL_DYNAMIC_READ
	DynamicCopy = 0x88EA  // GL_DYNAMIC_COPY
};

enum class BufferType {
	Vertex	= 0x8892, // GL_ARRAY_BUFFER
	Index	= 0x8893, // GL_ELEMENT_ARRAY_BUFFER
	Uniform = 0x8A11, // GL_UNIFORM_BUFFER
};

enum class BufferCategory {
	Color	= 0x1800, // GL_COLOR
	Depth	= 0x1801, // GL_DEPTH
	Stencil = 0x1802, // GL_STENCIL
};

enum class GLBinding {
	VertexArray		= 0x85B5, // GL_VERTEX_ARRAY_BINDING
	VertexBuffer	= 0x8894, // GL_ARRAY_BUFFER_BINDING
	IndexBuffer		= 0x8895, // GL_ELEMENT_ARRAY_BUFFER_BINDING
	UniformBuffer	= 0x8A28, // GL_UNIFORM_BUFFER_BINDING
	FrameBufferDraw = 0x8CA6, // GL_DRAW_FRAMEBUFFER_BINDING
	FrameBufferRead = 0x8CAA, // GL_READ_FRAMEBUFFER_BINDING
	RenderBuffer	= 0x8CA7, // GL_RENDERBUFFER_BINDING
	Texture2D		= 0x8069, // GL_TEXTURE_BINDING_2D
	ActiveUnit		= 0x84E0  // GL_ACTIVE_TEXTURE
};

template <BufferType T>
constexpr GLBinding GetGLBinding() {
	if constexpr (T == BufferType::Vertex) {
		return GLBinding::VertexBuffer;
	} else if constexpr (T == BufferType::Index) {
		return GLBinding::IndexBuffer;
	} else if constexpr (T == BufferType::Uniform) {
		return GLBinding::UniformBuffer;
	}
}

template <typename T>
concept VertexDataType = IsAnyOf<
	T, glsl::float_, glsl::vec2, glsl::vec3, glsl::vec4, glsl::double_, glsl::dvec2, glsl::dvec3,
	glsl::dvec4, glsl::bool_, glsl::bvec2, glsl::bvec3, glsl::bvec4, glsl::int_, glsl::ivec2,
	glsl::ivec3, glsl::ivec4, glsl::uint_, glsl::uvec2, glsl::uvec3, glsl::uvec4>;

enum class GLType : std::uint32_t {
	None			= 0,
	Byte			= 0x1400, // GL_BYTE
	UnsignedByte	= 0x1401, // GL_UNSIGNED_BYTE
	Short			= 0x1402, // GL_SHORT
	UnsignedShort	= 0x1403, // GL_UNSIGNED_SHORT
	Int				= 0x1404, // GL_INT
	UnsignedInt		= 0x1405, // GL_UNSIGNED_INT
	Float			= 0x1406, // GL_FLOAT
	Double			= 0x140A, // GL_DOUBLE
	UnsignedInt24_8 = 0x84FA  // GL_UNSIGNED_INT_24_8
};

template <typename T>
concept SupportedGLType = IsAnyOf<
	T, float, double, std::int32_t, std::uint32_t, std::int16_t, std::uint16_t, std::int8_t,
	std::uint8_t, bool>;

template <SupportedGLType T>
[[nodiscard]] constexpr GLType GetType() {
	if constexpr (std::is_same_v<T, float>) {
		return GLType::Float;
	} else if constexpr (std::is_same_v<T, double>) {
		return GLType::Double;
	} else if constexpr (std::is_same_v<T, std::int32_t>) {
		return GLType::Int;
	} else if constexpr (std::is_same_v<T, std::uint32_t>) {
		return GLType::UnsignedInt;
	} else if constexpr (std::is_same_v<T, std::int16_t>) {
		return GLType::Short;
	} else if constexpr (std::is_same_v<T, std::uint16_t>) {
		return GLType::UnsignedShort;
	} else if constexpr (std::is_same_v<T, std::int8_t> || std::is_same_v<T, bool>) {
		return GLType::Byte;
	} else if constexpr (std::is_same_v<T, std::uint8_t>) {
		return GLType::UnsignedByte;
	}
}

} // namespace ptgn::impl