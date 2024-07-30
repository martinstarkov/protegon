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

// Returns the max buffer size (as set by glBufferData) of the currently bound buffer.
[[nodiscard]] inline static std::uint32_t GetMaxBufferSize(BufferType type) {
	std::int32_t max_size{ 0 };
	gl::GetBufferParameteriv(static_cast<gl::GLenum>(type), GL_BUFFER_SIZE, &max_size);
	return static_cast<std::uint32_t>(max_size);
}

} // namespace impl

void VertexBuffer::SetDataImpl(const void* vertex_data, std::uint32_t size, BufferUsage usage) {
	Bind();
	gl::BufferData(
		static_cast<gl::GLenum>(impl::BufferType::Vertex), size, vertex_data,
		static_cast<gl::GLenum>(usage)
	);
}

void VertexBuffer::SetSubData(const void* vertex_data, std::uint32_t size) {
	Bind();
	// assert check must be done after buffer is bound
	PTGN_ASSERT(size <= impl::GetMaxBufferSize(impl::BufferType::Vertex));
	gl::BufferSubData(static_cast<gl::GLenum>(impl::BufferType::Vertex), 0, size, vertex_data);
}

void VertexBuffer::Bind() const {
	PTGN_CHECK(IsValid(), "Cannot bind uninitialized or destroyed vertex buffer");
	gl::BindBuffer(static_cast<gl::GLenum>(impl::BufferType::Vertex), instance_->id_);
}

void VertexBuffer::Unbind() const {
	gl::BindBuffer(static_cast<gl::GLenum>(impl::BufferType::Vertex), 0);
}

const impl::InternalBufferLayout& VertexBuffer::GetLayout() const {
	PTGN_CHECK(IsValid(), "Cannot get layout of uninitialized or destroyed vertex buffer");
	return instance_->layout_;
}

IndexBuffer::IndexBuffer(const void* index_data, std::uint32_t size) {
	if (!IsValid()) {
		instance_ = std::make_shared<impl::IndexBufferInstance>();
	}
	count_ = size / sizeof(impl::IndexType);
	SetDataImpl(index_data, size);
}

void IndexBuffer::SetDataImpl(const void* index_data, std::uint32_t size) {
	Bind();
	gl::BufferData(
		static_cast<gl::GLenum>(impl::BufferType::Index), size, index_data,
		static_cast<gl::GLenum>(BufferUsage::StaticDraw)
	);
}

void IndexBuffer::SetSubData(const void* index_data, std::uint32_t size) {
	Bind();
	PTGN_ASSERT(size <= count_ * sizeof(impl::IndexType));
	// assert check must be done after buffer is bound
	PTGN_ASSERT(size <= impl::GetMaxBufferSize(impl::BufferType::Index));
	gl::BufferSubData(static_cast<gl::GLenum>(impl::BufferType::Index), 0, size, index_data);
}

void IndexBuffer::Bind() const {
	PTGN_CHECK(IsValid(), "Cannot bind uninitialized or destroyed index buffer");
	gl::BindBuffer(static_cast<gl::GLenum>(impl::BufferType::Index), instance_->id_);
}

void IndexBuffer::Unbind() const {
	gl::BindBuffer(static_cast<gl::GLenum>(impl::BufferType::Index), 0);
}

std::uint32_t IndexBuffer::GetCount() const {
	return count_;
}

} // namespace ptgn
