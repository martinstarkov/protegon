#pragma once

#include <memory>
#include <vector>

#include "protegon/buffer.h"
#include "protegon/shader.h"

namespace ptgn {

class VertexArray;

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

	VertexArray(
		PrimitiveMode mode, const VertexBuffer& vertex_buffer, const IndexBuffer& index_buffer = {}
	);

	void Bind() const;
	void Unbind() const;

	void SetPrimitiveMode(PrimitiveMode mode);
	void SetVertexBuffer(const VertexBuffer& vertex_buffer);
	void SetIndexBuffer(const IndexBuffer& index_buffer);

	// Does not check VertexBuffer validity.
	[[nodiscard]] const VertexBuffer& GetVertexBuffer() const;

	// Does not check IndexBuffer validity.
	[[nodiscard]] const IndexBuffer& GetIndexBuffer() const;

	[[nodiscard]] PrimitiveMode GetPrimitiveMode() const;
};

} // namespace ptgn