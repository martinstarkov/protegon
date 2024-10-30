#pragma once

#include <cstdint>
#include <memory>

#include "renderer/buffer.h"
#include "renderer/buffer_layout.h"
#include "renderer/gl_helper.h"
#include "utility/debug.h"
#include "utility/handle.h"

namespace ptgn {

struct Color;
struct Rect;
class RenderTexture;

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
	) {
		Create();

		SetPrimitiveMode(mode);

		Bind();

		SetVertexBufferImpl(vertex_buffer);

		SetIndexBufferImpl(index_buffer);

		SetBufferLayoutImpl(layout);
	}

	VertexArray(const Rect& rect, const Color& color);
	explicit VertexArray(const RenderTexture& render_texture);

	void Draw() const;

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

		SetBufferLayoutImpl(layout);
	}

	[[nodiscard]] bool HasVertexBuffer() const;

	[[nodiscard]] bool HasIndexBuffer() const;

	// Note, returning by copy is okay since they are handles.

	[[nodiscard]] VertexBuffer GetVertexBuffer() const;
	[[nodiscard]] IndexBuffer GetIndexBuffer() const;

	[[nodiscard]] PrimitiveMode GetPrimitiveMode() const;

	// TODO: Move to private.
	void Bind() const;

private:
	template <BufferType BT>
	friend class Buffer;
	friend class GLRenderer;
	friend class RendererData;

	[[nodiscard]] static std::int32_t GetBoundId();
	[[nodiscard]] static bool WithinMaxAttributes(std::size_t attribute_count);

	void SetVertexBufferImpl(const VertexBuffer& vertex_buffer);
	void SetIndexBufferImpl(const IndexBuffer& index_buffer);

	void SetBufferElement(
		std::uint32_t index, const impl::BufferElement& element, std::int32_t stride
	);

	template <typename... Ts>
	void SetBufferLayoutImpl(const BufferLayout<Ts...>& layout) {
		PTGN_ASSERT(
			!layout.IsEmpty(),
			"Cannot add a vertex buffer with an empty (unset) layout to a vertex array"
		);

		const auto& elements = layout.GetElements();
		PTGN_ASSERT(WithinMaxAttributes(elements.size()), "Too many vertex attributes");

		auto stride{ layout.GetStride() };
		PTGN_ASSERT(stride > 0, "Failed to calculate buffer layout stride");

		for (std::uint32_t i{ 0 }; i < elements.size(); ++i) {
			SetBufferElement(i, elements[i], stride);
		}
	}

	static void Unbind();
};

} // namespace ptgn