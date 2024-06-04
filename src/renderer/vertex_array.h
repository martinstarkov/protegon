#pragma once

#include <memory>
#include <vector>

#include "buffer.h"

namespace ptgn {

class VertexArray {
public:
	using Id = std::uint32_t;

	static std::shared_ptr<VertexArray> Create();

	VertexArray();
	~VertexArray();

	void Bind() const;
	void Unbind() const;

	void AddVertexBuffer(const std::shared_ptr<AbstractVertexBuffer>& vertex_buffer);
	void SetIndexBuffer(const std::shared_ptr<IndexBuffer>& index_buffer);

	Id GetId() const;
private:
	std::vector<std::shared_ptr<AbstractVertexBuffer>> vertex_buffers_;
	std::shared_ptr<IndexBuffer> index_buffer_;
	Id id_{ 0 };
};

} // namespace ptgn