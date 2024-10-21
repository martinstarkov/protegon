#pragma once

#include <cstdint>
#include <memory>

#include "renderer/buffer.h"
#include "renderer/buffer_layout.h"
#include "renderer/gl_helper.h"
#include "utility/handle.h"

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
	VertexArray()			= default;
	~VertexArray() override = default;

	template <typename... Ts>
	VertexArray(
		PrimitiveMode mode, const VertexBuffer& vertex_buffer, const BufferLayout<Ts...>& layout,
		const IndexBuffer& index_buffer
	) :
		VertexArray{ mode, vertex_buffer, impl::InternalBufferLayout{ layout }, index_buffer } {}

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

		Create();

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
		PrimitiveMode mode, const VertexBuffer& vertex_buffer, impl::InternalBufferLayout layout,
		const IndexBuffer& index_buffer
	);

private:
	template <BufferType BT>
	friend class Buffer;
	friend class GLRenderer;
	friend class RendererData;

	[[nodiscard]] static std::int32_t GetBoundId();

	void SetVertexBufferImpl(const VertexBuffer& vertex_buffer);
	void SetIndexBufferImpl(const IndexBuffer& index_buffer);
	void SetLayoutImpl(const impl::InternalBufferLayout& layout) const;

	void Bind() const;
	static void Unbind();
};

} // namespace ptgn