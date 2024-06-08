#pragma once

#include <memory>
#include <vector>

#include "buffer.h"

namespace ptgn {

class VertexArray;

namespace impl {

struct VertexArrayInstance {
public:
	VertexArrayInstance();
	~VertexArrayInstance();
private:
	friend class VertexArray;

	std::vector<VertexBuffer> vertex_buffers_;
	IndexBuffer index_buffer_;
	std::uint32_t id_{ 0 };
};

} // namespace impl

class VertexArray : public Handle<impl::VertexArrayInstance> {
public:
	VertexArray() = default;
	~VertexArray() = default;
	[[nodiscard]] static VertexArray Create();

	void Bind() const;
	void Unbind() const;

	void AddVertexBuffer(const VertexBuffer& vertex_buffer);
	void SetIndexBuffer(const IndexBuffer& index_buffer);
	[[nodiscard]] const IndexBuffer& GetIndexBuffer() const;
private:
	VertexArray(const std::shared_ptr<impl::VertexArrayInstance>& instance);
};

} // namespace ptgn