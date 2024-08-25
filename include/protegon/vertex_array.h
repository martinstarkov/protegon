#pragma once

#include <memory>
#include <vector>

#include "protegon/buffer.h"
#include "renderer/buffer_layout.h"

namespace ptgn {

class GLRenderer;

namespace impl {

class RendererData;

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
	) :
		VertexArray{ mode, vertex_buffer, layout, index_buffer } {
		static_assert(
			(impl::is_vertex_data_type<Ts> && ...),
			"Provided vertex type should only contain ptgn::glsl:: types"
		);
		static_assert(sizeof...(Ts) > 0, "Must provide layout types as template arguments");
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

	// Note, returning by copy is okay since they are handles.

	[[nodiscard]] VertexBuffer GetVertexBuffer();
	[[nodiscard]] IndexBuffer GetIndexBuffer();

	[[nodiscard]] PrimitiveMode GetPrimitiveMode() const;

	VertexArray(
		PrimitiveMode mode, const VertexBuffer& vertex_buffer,
		const impl::InternalBufferLayout& layout, const IndexBuffer& index_buffer
	);

private:
	template <BufferType BT>
	friend class Buffer;
	friend class GLRenderer;
	friend class RendererData;

	static std::int32_t BoundId();

	void SetVertexBufferImpl(const VertexBuffer& vertex_buffer);
	void SetIndexBufferImpl(const IndexBuffer& index_buffer);
	void SetLayoutImpl(const impl::InternalBufferLayout& layout);

	void Bind() const;
	static void Unbind();
};

} // namespace ptgn