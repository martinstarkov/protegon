#pragma once

#include <memory>
#include <vector>

#include "buffer.h"
#include "shader.h"

namespace ptgn {

class VertexArray;

namespace impl {

struct VertexArrayInstance {
	VertexArrayInstance() = default;
	~VertexArrayInstance();
	VertexArrayInstance(PrimitiveMode mode);

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
		PrimitiveMode mode, const VertexBuffer& vertex_buffer = {},
		const IndexBuffer& index_buffer = {}
	);

	void Bind() const;
	void Unbind() const;

	void SetVertexBuffer(const VertexBuffer& vertex_buffer);
	void SetIndexBuffer(const IndexBuffer& index_buffer);

	// Does not check VertexBuffer validity.
	[[nodiscard]] const VertexBuffer& GetVertexBuffer() const;
	// Does not check IndexBuffer validity.
	[[nodiscard]] const IndexBuffer& GetIndexBuffer() const;

	[[nodiscard]] PrimitiveMode GetPrimitiveMode() const;
	void SetPrimitiveMode(PrimitiveMode mode);

private:
	VertexArray(const std::shared_ptr<impl::VertexArrayInstance>& instance);
};

} // namespace ptgn