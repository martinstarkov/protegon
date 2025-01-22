#pragma once

#include <cstdint>

#include "renderer/buffer.h"
#include "renderer/buffer_layout.h"
#include "renderer/gl_types.h"
#include "utility/debug.h"
#include "utility/handle.h"

namespace ptgn {

struct Color;
struct Rect;

class GLRenderer;

namespace impl {

struct TextureVertices;
class RendererData;

struct VertexArrayInstance {
	VertexArrayInstance();
	VertexArrayInstance(const VertexArrayInstance&)				   = default;
	VertexArrayInstance& operator=(const VertexArrayInstance&)	   = default;
	VertexArrayInstance(VertexArrayInstance&&) noexcept			   = default;
	VertexArrayInstance& operator=(VertexArrayInstance&&) noexcept = default;
	~VertexArrayInstance();

	PrimitiveMode mode_{ PrimitiveMode::Triangles };
	VertexBuffer vertex_buffer_;
	IndexBuffer index_buffer_;
	std::uint32_t id_{ 0 };
};

} // namespace impl

class VertexArray : public Handle<impl::VertexArrayInstance> {
public:
	VertexArray() = default;

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

	// @param index_count The number of indices within the vertex array to draw.
	// If set to 0, will use either the total size of the bound index buffer,
	// or if no index buffer is bound will use the total vertex count.
	// @param bind_vertex_array Whether or not to bind the vertex array for the draw call.
	// Setting this to false can reduce vertex array bind calls if it is already bound.
	void Draw(std::size_t index_count = 0, bool bind_vertex_array = true) const;

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

	// @return True if the vertex array is currently bound, false otherwise.
	[[nodiscard]] bool IsBound() const;

	// TODO: Move to private.
	void Bind() const;

private:
	template <BufferType BT>
	friend class Buffer;
	friend class GLRenderer;
	friend class RendererData;

	// @return Id of the currently bound vertex array.
	[[nodiscard]] static std::uint32_t GetBoundId();

	[[nodiscard]] static bool WithinMaxAttributes(std::int32_t attribute_count);

	void SetVertexBufferImpl(const VertexBuffer& vertex_buffer);
	void SetIndexBufferImpl(const IndexBuffer& index_buffer);

	void SetBufferElement(
		std::uint32_t index, const impl::BufferElement& element, std::int32_t stride
	) const;

	template <typename... Ts>
	void SetBufferLayoutImpl(const BufferLayout<Ts...>& layout) {
		PTGN_ASSERT(
			!layout.IsEmpty(),
			"Cannot add a vertex buffer with an empty (unset) layout to a vertex array"
		);

		const auto& elements = layout.GetElements();
		PTGN_ASSERT(
			WithinMaxAttributes(static_cast<std::int32_t>(elements.size())),
			"Too many vertex attributes"
		);

		auto stride{ layout.GetStride() };
		PTGN_ASSERT(stride > 0, "Failed to calculate buffer layout stride");

		for (std::uint32_t i{ 0 }; i < elements.size(); ++i) {
			SetBufferElement(i, elements[i], stride);
		}
	}

	// Bind specific id as current vertex array.
	// Note: Calling this outside of the VertexArray class may mess with the renderer as it keeps
	// track of the currently bound vertex array.
	static void Bind(std::uint32_t id);

	static void Unbind();
};

} // namespace ptgn