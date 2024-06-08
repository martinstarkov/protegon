#pragma once

#include <cstdlib>
#include <cstdint>
#include <vector>
#include <initializer_list>
#include <memory>
#include <variant>
#include <array>

#include "protegon/shader.h"
#include "protegon/type_traits.h"

#include "luple/type_loophole.h"

namespace ptgn {

class VertexBuffer;
class IndexBuffer;
class BufferLayout;

class BufferElement {
public:
	BufferElement(std::uint16_t size_of_element, std::uint16_t count, impl::GLEnumType type, bool normalized = false);
	//BufferElement(ShaderDataType data_type, bool normalized = false);
	[[nodiscard]] std::uint16_t GetSize() const;
	[[nodiscard]] std::uint16_t GetCount() const;
	[[nodiscard]] impl::GLEnumType GetType() const;
	[[nodiscard]] bool IsNormalized() const;
	[[nodiscard]] std::size_t GetOffset() const;
private:
	BufferElement() = default;
	friend class BufferLayout;
	std::uint16_t size_{ 0 };    // Number of elements x Size of element.
	std::uint16_t count_{ 0 };   // Number of elements
	impl::GLEnumType type_{ 0 }; // Type of buffer element (i.e. GL_FLOAT)
	// Set by BufferLayout.
	std::size_t offset_{ 0 };  // Number of bytes from start of buffer.
	bool normalized_{ false };   // Whether or not the buffer elements are normalized. See here for more info: https://registry.khronos.org/OpenGL-Refpages/es3.0/html/glVertexAttribPointer.xhtml
};

class BufferLayout {
public:
	BufferLayout() = default;
	BufferLayout(const std::initializer_list<BufferElement>& elements);
	BufferLayout(const std::vector<BufferElement>& elements);
	[[nodiscard]] const std::vector<BufferElement>& GetElements() const;
	[[nodiscard]] std::size_t GetStride() const;
	[[nodiscard]] bool IsEmpty() const;
private:
	void CalculateOffsets();
	std::vector<BufferElement> elements_;
	std::size_t stride_{ 0 };
};

// Vertex Types

namespace glsl {

using float_ = std::array<float, 1>;
using vec2 = std::array<float, 2>;
using vec3 = std::array<float, 3>;
using vec4 = std::array<float, 4>;

using double_ = std::array<double, 1>;
using dvec2 = std::array<double, 2>;
using dvec3 = std::array<double, 3>;
using dvec4 = std::array<double, 4>;

using bool_ = std::array<bool, 1>;
using bvec2 = std::array<bool, 2>;
using bvec3 = std::array<bool, 3>;
using bvec4 = std::array<bool, 4>;

using int_ = std::array<int, 1>;
using ivec2 = std::array<int, 2>;
using ivec3 = std::array<int, 3>;
using ivec4 = std::array<int, 4>;

using uint_ = std::array<unsigned int, 1>;
using uvec2 = std::array<unsigned int, 2>;
using uvec3 = std::array<unsigned int, 3>;
using uvec4 = std::array<unsigned int, 4>;

} // namespace glsl

namespace impl {

template <typename T>
inline constexpr bool is_vertex_data_type{
	type_traits::is_one_of_v<T,
	glsl::float_,  glsl::vec2,  glsl::vec3,  glsl::vec4,
	glsl::double_, glsl::dvec2, glsl::dvec3, glsl::dvec4,
	glsl::bool_,   glsl::bvec2, glsl::bvec3, glsl::bvec4,
	glsl::int_,    glsl::ivec2, glsl::ivec3, glsl::ivec4,
	glsl::uint_,   glsl::uvec2, glsl::uvec3, glsl::uvec4>
};

template<bool> struct vertex_data : std::false_type {};
template<>     struct vertex_data<true> : std::true_type {};

template <typename ...Ts>
struct is_vertex_data
	: vertex_data <(is_vertex_data_type<Ts> && ...)> {};

template <typename T>
inline constexpr bool is_valid_vertex{ type_traits::class_members::types<T>::apply<impl::is_vertex_data>::value };

template <typename T>
using valid_vertex = std::enable_if_t<is_valid_vertex<T>, bool>;

struct VertexBufferInstance {
public:
	VertexBufferInstance() = default;
	~VertexBufferInstance();
private:
	template <typename T, impl::valid_vertex<T> = true>
	VertexBufferInstance(const std::vector<T>& vertices) {
		PTGN_ASSERT(vertices.size() > 0);
		// Take any vertex element to figure out its layout.
		DeduceLayout<T>();
		GenerateBuffer((void*)vertices.data(), sizeof(T) * vertices.size());
	}
	
	/*template <typename T, impl::valid_vertex<T> = true>
	VertexBufferInstance(const std::vector<T>& vertices, const BufferLayout& layout) {
		PTGN_ASSERT(vertices.size() > 0);
		GenerateBuffer((void*)vertices.data(), sizeof(T) * vertices.size());
	}*/
private:
	void GenerateBuffer(void* vertex_data, std::size_t size);
	
	template <typename T, impl::valid_vertex<T> = true>
	void DeduceLayout() {
		using data_luple = type_traits::class_members::impl::luple_t<type_traits::class_members::as_type_list<T>>;
		T v{};
		auto& l = reinterpret_cast<data_luple&>(v);
		std::vector<BufferElement> elements;
		type_traits::class_members::impl::luple_do(l,
			[&] (auto& value) {
				using glsl_type = typename std::remove_reference_t<decltype(value)>;
				static_assert(is_vertex_data_type<glsl_type>, "Buffer element type must be a valid vertex data type");
				using element_type = glsl_type::value_type;

				elements.emplace_back(
					static_cast<std::uint16_t>(sizeof(element_type)),
					static_cast<std::uint16_t>(value.size()),
					GetType<element_type>()
				);
			}
		);
		layout_ = { elements };
	}
private:
	friend class VertexBuffer;
	BufferLayout layout_;
	std::uint32_t id_{ 0 };
};

struct IndexBufferInstance {
public:
	IndexBufferInstance() = default;
	~IndexBufferInstance();
private:
	IndexBufferInstance(const std::vector<std::uint32_t>& indices);

	void GenerateBuffer(void* index_data, std::size_t size);
private:
	friend class IndexBuffer;
	std::size_t count_{ 0 };
	std::uint32_t id_{ 0 };
};

} // namespace impl

class VertexBuffer : public Handle<impl::VertexBufferInstance> {
public:
	VertexBuffer() = default;

	template <typename T>
	VertexBuffer(const std::vector<T>& vertices) {
		static_assert(impl::is_valid_vertex<T>, "Provided vertex type should only contain ptgn::glsl:: types");
		if (vertices.size() == 0) throw std::exception("Cannot create a vertex buffer with no vertices");
		instance_ = std::shared_ptr<impl::VertexBufferInstance>(new impl::VertexBufferInstance(vertices));
	}

	/*template <typename T>
	VertexBuffer(const std::vector<T>& vertices, const BufferLayout& layout) {
		static_assert(impl::is_valid_vertex<T>, "Provided vertex type should only contain ptgn::glsl:: types");
		if (vertices.size() == 0) throw std::exception("Cannot create a vertex buffer with no vertices");
		if (layout.GetStride() != sizeof(T)) throw std::exception("Provided buffer layout does not match the provided vertex struct");
		instance_ = std::shared_ptr<impl::VertexBufferInstance>(new impl::VertexBufferInstance(vertices, layout));
	}*/

	void Bind() const;
	void Unbind() const;

	[[nodiscard]] const BufferLayout& GetLayout() const;
};

class IndexBuffer : public Handle<impl::IndexBufferInstance> {
public:
	IndexBuffer() = default;
	IndexBuffer(const std::vector<std::uint32_t>& indices);

	void Bind() const;
	void Unbind() const;

	[[nodiscard]] std::size_t GetCount() const;
	[[nodiscard]] static constexpr impl::GLEnumType GetType() {
		return impl::TYPE_UNSIGNED_INT;
	}
};

} // namespace ptgn