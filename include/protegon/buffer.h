#pragma once

#include <array>
#include <cstdint>
#include <initializer_list>
#include <vector>

#include "handle.h"
#include "protegon/debug.h"
#include "renderer/gl_helper.h"

namespace ptgn {

class VertexBuffer;
class IndexBuffer;

namespace impl {

class BufferLayout;

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

	[[nodiscard]] static constexpr GLSLType GetType() {
		return GLSLType::UnsignedInt;
	}

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

	template <typename T>
	void SetData(const std::vector<T>& vertices) {
		PTGN_CHECK(vertices.size() != 0, "Cannot create a vertex buffer with no vertices");
		PTGN_CHECK(IsValid(), "Cannot set data of uninitialized or destroyed vertex buffer");
		instance_->SetData(vertices);
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

	[[nodiscard]] std::int32_t GetCount() const;

	[[nodiscard]] const impl::BufferLayout& GetLayout() const;
};

class IndexBuffer : public Handle<impl::IndexBufferInstance> {
public:
	IndexBuffer() = default;
	IndexBuffer(const std::initializer_list<std::uint32_t>& indices);

	void Bind() const;
	void Unbind() const;

	[[nodiscard]] std::int32_t GetCount() const;
};

} // namespace ptgn