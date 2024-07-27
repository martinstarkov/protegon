#pragma once

#include <array>
#include <cstdint>
#include <initializer_list>
#include <tuple>
#include <vector>

#include "protegon/buffer_layout.h"
#include "renderer/gl_helper.h"
#include "utility/debug.h"
#include "utility/handle.h"

namespace ptgn {

class VertexArray;
class GLRenderer;
class Renderer;

namespace impl {

struct VertexBufferInstance {
	VertexBufferInstance();
	~VertexBufferInstance();
	std::uint32_t id_{ 0 };
	impl::BufferLayout layout_;
};

struct IndexBufferInstance {
	IndexBufferInstance();
	~IndexBufferInstance();
	std::uint32_t id_{ 0 };
};

} // namespace impl

class VertexBuffer : public Handle<impl::VertexBufferInstance> {
public:
	VertexBuffer()	= default;
	~VertexBuffer() = default;

	template <typename T>
	VertexBuffer(const std::vector<T>& vertices) :
		VertexBuffer{ vertices.data(), static_cast<std::uint32_t>(vertices.size() * sizeof(T)) } {}

	VertexBuffer(
		const void* vertex_data, std::uint32_t size, BufferUsage usage = BufferUsage::StaticDraw
	);

	void SetData(
		const void* vertex_data, std::uint32_t size, BufferUsage usage = BufferUsage::StaticDraw
	);
	void SetSubData(const void* vertex_data, std::uint32_t size);

	void Bind() const;
	void Unbind() const;

	template <typename... Ts, type_traits::enable<(sizeof...(Ts) > 0)> = true>
	void SetLayout() {
		static_assert(
			(impl::is_vertex_data_type<Ts> && ...),
			"Provided vertex type should only contain ptgn::glsl:: types"
		);
		static_assert(sizeof...(Ts) > 0, "Must provide layout types as template arguments");
		PTGN_ASSERT(IsValid() && "Cannot set layout of uninitialized or destroyed vertex buffer");
		instance_->layout_ = CalculateLayout<Ts...>();
	}

	[[nodiscard]] const impl::BufferLayout& GetLayout() const;

private:
	friend class VertexArray;

	template <typename... Ts>
	constexpr std::array<impl::BufferElement, sizeof...(Ts)> CalculateLayout() {
		return {
			impl::BufferElement{
								static_cast<std::uint16_t>(sizeof(Ts) / std::tuple_size<Ts>::value),
								static_cast<std::uint16_t>(std::tuple_size<Ts>::value),
								impl::GetType<typename Ts::value_type>()}
			  ...
		};
	}
};

class IndexBuffer : public Handle<impl::IndexBufferInstance> {
public:
	using IndexType = std::uint32_t;
	using Indices	= std::vector<IndexType>;

	IndexBuffer()  = default;
	~IndexBuffer() = default;

	IndexBuffer(const Indices& indices);

	void SetData(const Indices& indices);
	void SetSubData(const Indices& indices);

	void Bind() const;
	void Unbind() const;

	std::uint32_t GetCount() const;

private:
	friend class GLRenderer;

	constexpr static impl::GLType GetType() {
		return impl::GetType<IndexType>();
	}

	std::uint32_t count_{ 0 };
};

} // namespace ptgn