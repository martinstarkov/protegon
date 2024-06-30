#pragma once

#include <array>
#include <cstdint>
#include <initializer_list>
#include <vector>

#include "protegon/debug.h"
#include "protegon/handle.h"
#include "protegon/type_traits.h"

namespace ptgn {

class VertexBuffer;
class IndexBuffer;
class BufferElement;
class BufferLayout;

enum class PrimitiveMode : std::uint32_t {
	Points		  = 0x0000, // GL_POINTS
	Lines		  = 0x0001, // GL_LINES
	LineLoop	  = 0x0002, // GL_LINE_LOOP
	LineStrip	  = 0x0003, // GL_LINE_STRIP
	Triangles	  = 0x0004, // GL_TRIANGLES
	TriangleStrip = 0x0005, // GL_TRIANGLE_STRIP
	TriangleFan	  = 0x0006, // GL_TRIANGLE_FAN
	Quads		  = 0x0007, // GL_QUADS
	QuadStrip	  = 0x0008, // GL_QUAD_STRIP
	Polygon		  = 0x0009	// GL_POLYGON
};

namespace impl {

template <typename T>
inline constexpr bool is_vertex_data_type{ type_traits::is_one_of_v<
	T, glsl::float_, glsl::vec2, glsl::vec3, glsl::vec4, glsl::double_, glsl::dvec2, glsl::dvec3,
	glsl::dvec4, glsl::bool_, glsl::bvec2, glsl::bvec3, glsl::bvec4, glsl::int_, glsl::ivec2,
	glsl::ivec3, glsl::ivec4, glsl::uint_, glsl::uvec2, glsl::uvec3, glsl::uvec4> };

enum class GLSLType : std::uint32_t {
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
[[nodiscard]] GLSLType GetType() {
	static_assert(
		type_traits::is_one_of_v<
			T, float, double, std::int32_t, std::uint32_t, std::int16_t, std::uint16_t, std::int8_t,
			std::uint8_t, bool>,
		"Cannot retrieve type which is not supported by OpenGL"
	);

	if constexpr (std::is_same_v<T, float>) {
		return GLSLType::Float;
	} else if constexpr (std::is_same_v<T, double>) {
		return GLSLType::Double;
	} else if constexpr (std::is_same_v<T, std::int32_t>) {
		return GLSLType::Int;
	} else if constexpr (std::is_same_v<T, std::uint32_t>) {
		return GLSLType::UnsignedInt;
	} else if constexpr (std::is_same_v<T, std::int16_t>) {
		return GLSLType::Short;
	} else if constexpr (std::is_same_v<T, std::uint16_t>) {
		return GLSLType::UnsignedShort;
	} else if constexpr (std::is_same_v<T, std::int8_t> || std::is_same_v<T, bool>) {
		return GLSLType::Byte;
	} else if constexpr (std::is_same_v<T, std::uint8_t>) {
		return GLSLType::UnsignedByte;
	}
}

} // namespace impl

class BufferElement {
public:
	BufferElement() = default;
	BufferElement(
		std::uint16_t size_of_element, std::uint16_t count, impl::GLSLType type,
		bool normalized = false
	);
	// BufferElement(ShaderDataType data_type, bool normalized = false);
	[[nodiscard]] std::uint16_t GetSize() const;
	[[nodiscard]] std::uint16_t GetCount() const;
	[[nodiscard]] impl::GLSLType GetType() const;
	[[nodiscard]] bool IsNormalized() const;
	[[nodiscard]] std::size_t GetOffset() const;

private:
	friend class BufferLayout;
	std::uint16_t size_{ 0 };  // Number of elements x Size of element.
	std::uint16_t count_{ 0 }; // Number of elements
	impl::GLSLType type_{ 0 }; // Type of buffer element (i.e. GL_FLOAT)
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

using int_	= std::array<int, 1>;
using ivec2 = std::array<int, 2>;
using ivec3 = std::array<int, 3>;
using ivec4 = std::array<int, 4>;

using uint_ = std::array<unsigned int, 1>;
using uvec2 = std::array<unsigned int, 2>;
using uvec3 = std::array<unsigned int, 3>;
using uvec4 = std::array<unsigned int, 4>;

} // namespace glsl

namespace impl {

struct VertexBufferInstance {
	VertexBufferInstance() = default;
	~VertexBufferInstance();

	template <typename T>
	VertexBufferInstance(const std::vector<T>& vertices) :
		count_{ static_cast<std::int32_t>(vertices.size()) } {
		PTGN_ASSERT(count_ > 0);
		GenerateBuffer((void*)vertices.data(), sizeof(T) * count_);
	}

	void GenerateBuffer(void* vertex_data, std::size_t size);
	void GenerateBuffer(std::size_t size);

	void SetBuffer(void* vertex_data, std::size_t size);

	template <typename... Ts>
	void SetLayout() {
		static_assert(
			(is_vertex_data_type<Ts> && ...),
			"Provided vertex type should only contain ptgn::glsl:: types"
		);
		static_assert(sizeof...(Ts) > 0, "Must provide layout types as template arguments");
		layout_ = CalculateLayout<Ts...>();
	}

	template <typename T>
	void SetData(const std::vector<T>& vertices) {
		// TODO: Consider in the future adding offset capability.
		PTGN_ASSERT(static_cast<std::int32_t>(vertices.size()) == count_);
		SetBuffer((void*)vertices.data(), sizeof(T) * count_);
	}

	template <typename... Ts>
	constexpr std::array<BufferElement, sizeof...(Ts)> CalculateLayout() {
		std::array<BufferElement, sizeof...(Ts)> elements = {
			BufferElement{static_cast<std::uint16_t>(sizeof(Ts) / std::tuple_size<Ts>::value),
						   static_cast<std::uint16_t>(std::tuple_size<Ts>::value),
						   GetType<typename Ts::value_type>()}
			   ...
		};
		return elements;
	}

	std::int32_t count_{ 0 };
	BufferLayout layout_;
	std::uint32_t id_{ 0 };
};

struct IndexBufferInstance {
	IndexBufferInstance() = default;
	~IndexBufferInstance();
	IndexBufferInstance(const std::vector<std::uint32_t>& indices);
	void GenerateBuffer(void* index_data, std::size_t size);

	std::int32_t count_{ 0 };
	std::uint32_t id_{ 0 };
};

} // namespace impl

class VertexBuffer : public Handle<impl::VertexBufferInstance> {
public:
	VertexBuffer() = default;

	template <typename T>
	VertexBuffer(const std::vector<T>& vertices) {
		PTGN_CHECK(vertices.size() != 0, "Cannot create a vertex buffer with no vertices");
		instance_ =
			std::shared_ptr<impl::VertexBufferInstance>(new impl::VertexBufferInstance(vertices));
	}

	template <typename... Ts, type_traits::enable<(sizeof...(Ts) > 0)> = true>
	void SetLayout() {
		static_assert(
			(impl::is_vertex_data_type<Ts> && ...),
			"Provided vertex type should only contain ptgn::glsl:: types"
		);
		PTGN_ASSERT(instance_ != nullptr);
		instance_->SetLayout<Ts...>();
	}

	void Bind() const;
	void Unbind() const;

	[[nodiscard]] const BufferLayout& GetLayout() const;

	[[nodiscard]] std::int32_t GetCount() const;
};

class IndexBuffer : public Handle<impl::IndexBufferInstance> {
public:
	IndexBuffer() = default;
	IndexBuffer(const std::initializer_list<std::uint32_t>& indices);

	void Bind() const;
	void Unbind() const;

	[[nodiscard]] std::int32_t GetCount() const;

	[[nodiscard]] static constexpr impl::GLSLType GetType() {
		return impl::GLSLType::UnsignedInt;
	}
};

} // namespace ptgn