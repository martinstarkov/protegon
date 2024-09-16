#pragma once

#include <array>
#include <cstdint>

#include "core/gl_context.h"
#include "utility/debug.h"
#include "utility/type_traits.h"

namespace ptgn {

namespace impl {

#ifdef PTGN_DEBUG

#define GLCall(x)                                                     \
	std::invoke([&, fn = PTGN_FUNCTION_NAME()]() {                    \
		ptgn::impl::GLContext::ClearErrors();                         \
		x;                                                            \
		auto errors = ptgn::impl::GLContext::GetErrors();             \
		if (errors.size() > 0) {                                      \
			ptgn::impl::GLContext::PrintErrors(                       \
				fn, std::filesystem::path(__FILE__), __LINE__, errors \
			);                                                        \
			PTGN_EXCEPTION("OpenGL Error");                           \
		}                                                             \
	})
#define GLCallReturn(x)                                               \
	std::invoke([&, fn = PTGN_FUNCTION_NAME()]() {                    \
		ptgn::impl::GLContext::ClearErrors();                         \
		auto value	= x;                                              \
		auto errors = ptgn::impl::GLContext::GetErrors();             \
		if (errors.size() > 0) {                                      \
			ptgn::impl::GLContext::PrintErrors(                       \
				fn, std::filesystem::path(__FILE__), __LINE__, errors \
			);                                                        \
			PTGN_EXCEPTION("OpenGL Error");                           \
		}                                                             \
		return value;                                                 \
	})

#else

#define GLCall(x)		x
#define GLCallReturn(x) x

#endif

} // namespace impl

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

namespace impl {

enum class GLBinding {
	VertexArray	  = 0x85B5, // GL_VERTEX_ARRAY_BINDING
	VertexBuffer  = 0x8894, // GL_ARRAY_BUFFER_BINDING
	IndexBuffer	  = 0x8895, // GL_ELEMENT_ARRAY_BUFFER_BINDING
	UniformBuffer = 0x8A28, // GL_UNIFORM_BUFFER_BINDING
	Texture2D	  = 0x8069	// GL_TEXTURE_BINDING_2D
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
inline constexpr bool is_vertex_data_type{ tt::is_any_of_v<
	T, glsl::float_, glsl::vec2, glsl::vec3, glsl::vec4, glsl::double_, glsl::dvec2, glsl::dvec3,
	glsl::dvec4, glsl::bool_, glsl::bvec2, glsl::bvec3, glsl::bvec4, glsl::int_, glsl::ivec2,
	glsl::ivec3, glsl::ivec4, glsl::uint_, glsl::uvec2, glsl::uvec3, glsl::uvec4> };

enum class GLType : std::uint32_t {
	None		  = 0,
	Byte		  = 0x1400, // GL_BYTE
	UnsignedByte  = 0x1401, // GL_UNSIGNED_BYTE
	Short		  = 0x1402, // GL_SHORT
	UnsignedShort = 0x1403, // GL_UNSIGNED_SHORT
	Int			  = 0x1404, // GL_INT
	UnsignedInt	  = 0x1405, // GL_UNSIGNED_INT
	Float		  = 0x1406, // GL_FLOAT
	Double		  = 0x140A, // GL_DOUBLE
};

template <typename T>
[[nodiscard]] constexpr GLType GetType() {
	static_assert(
		tt::is_any_of_v<
			T, float, double, std::int32_t, std::uint32_t, std::int16_t, std::uint16_t, std::int8_t,
			std::uint8_t, bool>,
		"Cannot retrieve type which is not supported by OpenGL"
	);

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

} // namespace impl

} // namespace ptgn