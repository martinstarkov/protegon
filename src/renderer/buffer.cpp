#include "protegon/buffer.h"

#include "renderer/gl_loader.h"

namespace ptgn {

namespace impl {

VertexBufferInstance::VertexBufferInstance() {
	gl::GenBuffers(1, &id_);
}

VertexBufferInstance::~VertexBufferInstance() {
	gl::DeleteBuffers(1, &id_);
}

IndexBufferInstance::IndexBufferInstance() {
	gl::GenBuffers(1, &id_);
}

IndexBufferInstance::~IndexBufferInstance() {
	gl::DeleteBuffers(1, &id_);
}

} // namespace impl

VertexBuffer::VertexBuffer(const void* vertex_data, std::uint32_t size, BufferUsage usage) {
	SetData(vertex_data, size, usage);
}

void VertexBuffer::Bind() const {
	PTGN_CHECK(IsValid(), "Cannot bind uninitialized or destroyed vertex buffer");
	gl::BindBuffer(static_cast<gl::GLenum>(impl::BufferType::Vertex), instance_->id_);
}

void VertexBuffer::Unbind() const {
	gl::BindBuffer(static_cast<gl::GLenum>(impl::BufferType::Vertex), 0);
}

void VertexBuffer::SetSubData(const void* vertex_data, std::uint32_t size) {
	// Check that size <= the allocated buffer size.
	PTGN_ASSERT([&]() -> bool {
		std::int32_t max_size{ 0 };
		gl::GetBufferParameteriv(
			static_cast<gl::GLenum>(impl::BufferType::Vertex), GL_BUFFER_SIZE, &max_size
		);
		return size <= max_size;
	}());
	Bind();
	gl::BufferSubData(static_cast<gl::GLenum>(impl::BufferType::Vertex), 0, size, vertex_data);
}

void VertexBuffer::SetData(const void* vertex_data, std::uint32_t size, BufferUsage usage) {
	if (!IsValid()) {
		instance_ = std::make_shared<impl::VertexBufferInstance>();
	}
	Bind();
	gl::BufferData(
		static_cast<gl::GLenum>(impl::BufferType::Vertex), size, vertex_data,
		static_cast<gl::GLenum>(usage)
	);
}

const impl::BufferLayout& VertexBuffer::GetLayout() const {
	PTGN_CHECK(IsValid(), "Cannot get layout of uninitialized or destroyed vertex buffer");
	return instance_->layout_;
}

IndexBuffer::IndexBuffer(const IndexBuffer::Indices& indices) {
	SetData(indices);
}

void IndexBuffer::Bind() const {
	PTGN_CHECK(IsValid(), "Cannot bind uninitialized or destroyed index buffer");
	gl::BindBuffer(static_cast<gl::GLenum>(impl::BufferType::Index), instance_->id_);
}

void IndexBuffer::Unbind() const {
	gl::BindBuffer(static_cast<gl::GLenum>(impl::BufferType::Index), 0);
}

void IndexBuffer::SetSubData(const IndexBuffer::Indices& indices) {
	PTGN_ASSERT(indices.size() <= count_);
	Bind();
	gl::BufferSubData(
		static_cast<gl::GLenum>(impl::BufferType::Index), 0, indices.size() * sizeof(std::uint32_t),
		indices.data()
	);
}

void IndexBuffer::SetData(const IndexBuffer::Indices& indices) {
	if (!IsValid()) {
		instance_ = std::make_shared<impl::IndexBufferInstance>();
	}
	count_ = indices.size();
	Bind();
	gl::BufferData(
		static_cast<gl::GLenum>(impl::BufferType::Index), indices.size() * sizeof(std::uint32_t),
		indices.data(), static_cast<gl::GLenum>(BufferUsage::StaticDraw)
	);
}

std::uint32_t IndexBuffer::GetCount() const {
	return count_;
}

} // namespace ptgn
