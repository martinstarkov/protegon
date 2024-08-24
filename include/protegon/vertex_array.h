#pragma once

#include <memory>
#include <vector>

#include "protegon/buffer.h"
#include "renderer/buffer_layout.h"

namespace ptgn {

class GLRenderer;

namespace impl {

struct VertexArrayInstance {
	VertexArrayInstance();
	~VertexArrayInstance();

	PrimitiveMode mode_{ PrimitiveMode::Triangles };
	VertexBuffer vertex_buffer_;
	IndexBuffer index_buffer_;
	std::uint32_t id_{ 0 };
};

} // namespace impl

class VertexArray : public Handle<impl::VertexArrayInstance> {
public:
	VertexArray()  = default;
	~VertexArray() = default;

	template <typename... Ts>
	VertexArray(
		PrimitiveMode mode, const VertexBuffer& vertex_buffer, const BufferLayout<Ts...>& layout,
		const IndexBuffer& index_buffer
	) {
		static_assert(
			(impl::is_vertex_data_type<Ts> && ...),
			"Provided vertex type should only contain ptgn::glsl:: types"
		);
		static_assert(sizeof...(Ts) > 0, "Must provide layout types as template arguments");

		if (!IsValid()) {
			instance_ = std::make_shared<impl::VertexArrayInstance>();
		}

		SetPrimitiveMode(mode);

		Bind();

		SetVertexBufferImpl(vertex_buffer);

		SetIndexBufferImpl(index_buffer);

		SetLayoutImpl(layout);
	}

	void SetPrimitiveMode(PrimitiveMode mode);
	void SetVertexBuffer(const VertexBuffer& vertex_buffer);
	void SetIndexBuffer(const IndexBuffer& index_buffer);

	template <typename... Ts>
	void SetLayout(const BufferLayout<Ts...>& layout) {
		static_assert(
			(impl::is_vertex_data_type<Ts> && ...),
			"Provided vertex type should only contain ptgn::glsl:: types"
		);
		static_assert(sizeof...(Ts) > 0, "Must provide layout types as template arguments");
		if (!IsValid()) {
			instance_ = std::make_shared<impl::VertexArrayInstance>();
		}

		Bind();

		SetLayoutImpl(layout);
	}

	[[nodiscard]] bool HasVertexBuffer() const;

	[[nodiscard]] bool HasIndexBuffer() const;

	[[nodiscard]] PrimitiveMode GetPrimitiveMode() const;

private:
	template <BufferType BT>
	friend class Buffer;
	friend class GLRenderer;

	static std::int32_t BoundId();

	void SetVertexBufferImpl(const VertexBuffer& vertex_buffer);
	void SetIndexBufferImpl(const IndexBuffer& index_buffer);
	void SetLayoutImpl(const impl::InternalBufferLayout& layout);

	void Bind() const;
	static void Unbind();
};

} // namespace ptgn