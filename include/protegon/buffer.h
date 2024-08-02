#pragma once

#include <array>
#include <vector>

#include "renderer/buffer_layout.h"
#include "renderer/gl_helper.h"
#include "utility/debug.h"
#include "utility/handle.h"
#include "utility/type_traits.h"

namespace ptgn {

class VertexArray;
class GLRenderer;

namespace impl {

struct VertexBufferInstance {
	VertexBufferInstance();
	~VertexBufferInstance();
	std::uint32_t id_{ 0 };
	impl::InternalBufferLayout layout_;
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

	template <typename... Ts>
	VertexBuffer(
		const void* vertex_data, std::uint32_t size, const BufferLayout<Ts...>& layout,
		BufferUsage usage = BufferUsage::StaticDraw
	) {
		static_assert(
			(impl::is_vertex_data_type<Ts> && ...),
			"Provided vertex type should only contain ptgn::glsl:: types"
		);
		static_assert(sizeof...(Ts) > 0, "Must provide layout types as template arguments");
		if (!IsValid()) {
			instance_ = std::make_shared<impl::VertexBufferInstance>();
		}
		PTGN_ASSERT(IsValid() && "Cannot set layout of uninitialized or destroyed vertex buffer");
		instance_->layout_ = layout;
		SetDataImpl(vertex_data, size, usage);
	}

	template <typename... Ts, typename T>
	VertexBuffer(
		const std::vector<T>& vertices, const BufferLayout<Ts...>& layout,
		BufferUsage usage = BufferUsage::StaticDraw
	) :
		VertexBuffer{ vertices.data(), static_cast<std::uint32_t>(vertices.size() * sizeof(T)),
					  layout, usage } {}

	template <typename... Ts, typename T, std::size_t I>
	VertexBuffer(
		const std::array<T, I>& vertices, const BufferLayout<Ts...>& layout,
		BufferUsage usage = BufferUsage::StaticDraw
	) :
		VertexBuffer{ vertices.data(), static_cast<std::uint32_t>(vertices.size() * sizeof(T)),
					  layout, usage } {
		static_assert(I > 0, "Must provide at least one vertex");
	}

	void SetSubData(const void* vertex_data, std::uint32_t size);

	template <typename T>
	void SetSubData(const std::vector<T>& vertices) {
		PTGN_ASSERT(vertices.size() > 0, "Must provide at least one vertex");
		SetSubData(vertices.data(), static_cast<std::uint32_t>(vertices.size() * sizeof(T)));
	}

	template <typename T, std::size_t I>
	void SetSubData(const std::array<T, I>& vertices) {
		static_assert(I > 0, "Must provide at least one vertex");
		SetSubData(vertices.data(), static_cast<std::uint32_t>(vertices.size() * sizeof(T)));
	}

private:
	friend class VertexArray;

	static std::int32_t BoundId();

	void Bind() const;
	// static void Unbind();

	[[nodiscard]] const impl::InternalBufferLayout& GetLayout() const;

	void SetDataImpl(const void* vertex_data, std::uint32_t size, BufferUsage usage);
};

class IndexBuffer : public Handle<impl::IndexBufferInstance> {
public:
	using IndexType = std::uint32_t;

	IndexBuffer()  = default;
	~IndexBuffer() = default;

	IndexBuffer(const void* index_data, std::uint32_t size);

	IndexBuffer(const std::vector<IndexType>& indices) :
		IndexBuffer{ indices.data(),
					 static_cast<std::uint32_t>(indices.size() * sizeof(IndexType)) } {}

	template <std::size_t I>
	IndexBuffer(const std::array<IndexType, I>& indices) :
		IndexBuffer{ indices.data(),
					 static_cast<std::uint32_t>(indices.size() * sizeof(IndexType)) } {
		static_assert(I > 0, "Must provide at least one index");
	}

	void SetSubData(const void* index_data, std::uint32_t size);

	void SetSubData(const std::vector<IndexType>& indices) {
		PTGN_ASSERT(indices.size() > 0, "Must provide at least one index");
		SetSubData(indices.data(), static_cast<std::uint32_t>(indices.size() * sizeof(IndexType)));
	}

	template <std::size_t I>
	void SetSubData(const std::array<IndexType, I>& indices) {
		static_assert(I > 0, "Must provide at least one index");
		SetSubData(indices.data(), static_cast<std::uint32_t>(indices.size() * sizeof(IndexType)));
	}

	std::uint32_t GetCount() const;

private:
	friend class GLRenderer;
	friend class VertexArray;

	static std::int32_t BoundId();

	void Bind() const;
	// static void Unbind();

	void SetDataImpl(const void* index_data, std::uint32_t size);

	constexpr static impl::GLType GetType() {
		return impl::GetType<IndexType>();
	}

	std::uint32_t count_{ 0 };
};

} // namespace ptgn