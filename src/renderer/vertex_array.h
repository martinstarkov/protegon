#pragma once

#include <memory>
#include <vector>

#include "buffer.h"

namespace ptgn {

class VertexArray;

namespace impl {

struct VertexArrayInstance {
public:
	VertexArrayInstance() = default;
	~VertexArrayInstance();
private:
	VertexArrayInstance(PrimitiveMode mode);
private:
	friend class VertexArray;

	PrimitiveMode mode_{ PrimitiveMode::Triangles };
	VertexBuffer vertex_buffer_;
	IndexBuffer index_buffer_;
	std::uint32_t id_{ 0 };
};

} // namespace impl

class VertexArray : public Handle<impl::VertexArrayInstance> {
public:
	VertexArray() = default;
	~VertexArray() = default;

	VertexArray(PrimitiveMode mode);

	void Bind() const;
	void Unbind() const;

	void Draw() const;

	void SetVertexBuffer(const VertexBuffer& vertex_buffer);
	void SetIndexBuffer(const IndexBuffer& index_buffer);
	[[nodiscard]] const VertexBuffer& GetVertexBuffer() const;
	[[nodiscard]] const IndexBuffer& GetIndexBuffer() const;

	[[nodiscard]] PrimitiveMode GetPrimitiveMode() const;
	void SetPrimitiveMode(PrimitiveMode mode);
private:
	VertexArray(const std::shared_ptr<impl::VertexArrayInstance>& instance);
};

} // namespace ptgn