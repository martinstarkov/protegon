#pragma once

#include <cstdint>
#include <memory>

#include "renderer/buffer.h"
#include "renderer/buffer_layout.h"
#include "renderer/gl_types.h"
#include "utility/assert.h"

namespace ptgn::impl {

class VertexArray {
public:
	VertexArray() = default;

	template <typename... Ts>
	VertexArray(
		PrimitiveMode mode, VertexBuffer&& vertex_buffer,
		const BufferLayout<Ts...>& vertex_buffer_layout, IndexBuffer&& index_buffer
	) {
		GenerateVertexArray();
		SetPrimitiveMode(mode);
		Bind();
		SetVertexBuffer(std::move(vertex_buffer));
		SetIndexBuffer(std::move(index_buffer));
		SetVertexBufferLayout(vertex_buffer_layout);
	}

	VertexArray(const VertexArray&)			   = delete;
	VertexArray& operator=(const VertexArray&) = delete;
	VertexArray(VertexArray&& other) noexcept;
	VertexArray& operator=(VertexArray&& other) noexcept;
	~VertexArray();

	bool operator==(const VertexArray& other) const;
	bool operator!=(const VertexArray& other) const;

	void SetPrimitiveMode(PrimitiveMode mode);
	void SetVertexBuffer(VertexBuffer&& new_vertex_buffer);
	void SetIndexBuffer(IndexBuffer&& new_index_buffer);

	template <typename... Ts>
	void SetVertexBufferLayout(const BufferLayout<Ts...>& layout) {
		static_assert(
			(is_vertex_data_type<Ts> && ...),
			"Provided vertex type should only contain ptgn::glsl:: types"
		);
		static_assert(sizeof...(Ts) > 0, "Must provide layout types as template arguments");

		PTGN_ASSERT(
			!layout.IsEmpty(),
			"Cannot add a vertex buffer with an empty (unset) layout to a vertex array"
		);

		const auto& elements{ layout.GetElements() };
		PTGN_ASSERT(
			elements.size() < GetMaxAttributes(),
			"Vertex buffer layout cannot exceed maximum number of vertex array attributes"
		);

		auto stride{ layout.GetStride() };
		PTGN_ASSERT(stride > 0, "Failed to calculate buffer layout stride");

		for (std::uint32_t i{ 0 }; i < elements.size(); ++i) {
			SetBufferElement(i, elements[i], stride);
		}
	}

	[[nodiscard]] bool HasVertexBuffer() const;
	[[nodiscard]] bool HasIndexBuffer() const;

	[[nodiscard]] const VertexBuffer& GetVertexBuffer() const;
	[[nodiscard]] VertexBuffer& GetVertexBuffer();
	[[nodiscard]] const IndexBuffer& GetIndexBuffer() const;
	[[nodiscard]] IndexBuffer& GetIndexBuffer();

	[[nodiscard]] PrimitiveMode GetPrimitiveMode() const;

	// @return The maximum number of vertex array attributes which can be specified.
	[[nodiscard]] static std::uint32_t GetMaxAttributes();

	// @return True if the vertex array is currently bound, false otherwise.
	[[nodiscard]] bool IsBound() const;

	void Bind() const;

	// @return Id of the currently bound vertex array.
	[[nodiscard]] static std::uint32_t GetBoundId();

	// Bind specific id as current vertex array.
	static void Bind(std::uint32_t id);

	static void Unbind();

	// @return True if id != 0.
	[[nodiscard]] bool IsValid() const;

private:
	void GenerateVertexArray();
	void DeleteVertexArray() noexcept;

	void SetBufferElement(std::uint32_t index, const BufferElement& element, std::int32_t stride)
		const;

	std::uint32_t id_{ 0 };
	PrimitiveMode mode_{ PrimitiveMode::Triangles };
	VertexBuffer vertex_buffer_;
	IndexBuffer index_buffer_;
};

} // namespace ptgn::impl