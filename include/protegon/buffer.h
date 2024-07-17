#pragma once

#include <array>
#include <cstdint>
#include <initializer_list>
#include <tuple>
#include <vector>

#include "protegon/buffer_layout.h"
#include "utility/handle.h"
#include "utility/debug.h"

namespace ptgn {

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

	static std::uint32_t GetType();

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