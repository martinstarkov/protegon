#include "rendering/buffers/buffer.h"

#include <cstdint>

#include "rendering/gl/gl_helper.h"
#include "rendering/gl/gl_loader.h"
#include "rendering/buffers/vertex_array.h"
#include "common/assert.h"
#include "debug/stats.h"

namespace ptgn::impl {

template <BufferType BT>
Buffer<BT>::Buffer(
	const void* data, std::uint32_t element_count, std::uint32_t element_size, BufferUsage usage
) {
	GenerateBuffer();

	PTGN_ASSERT(element_count > 0, "Number of buffer elements must be greater than 0");
	PTGN_ASSERT(element_size > 0, "Byte size of a buffer element must be greater than 0");

	count_ = element_count;
	// Ensure that this buffer does not get bound to any currently bound vertex array.
	VertexArray::Unbind();

	std::uint32_t size{ element_count * element_size };

	Bind();

	GLCall(gl::BufferData(static_cast<gl::GLenum>(BT), size, data, static_cast<gl::GLenum>(usage)));
}

template <BufferType BT>
Buffer<BT>::~Buffer() {
	DeleteBuffer();
}

template <BufferType BT>
bool Buffer<BT>::operator==(const Buffer& other) const {
	return id_ == other.id_;
}

template <BufferType BT>
bool Buffer<BT>::operator!=(const Buffer& other) const {
	return !(*this == other);
}

template <BufferType BT>
Buffer<BT>::Buffer(Buffer&& other) noexcept :
	id_{ std::exchange(other.id_, 0) }, count_{ std::exchange(other.count_, 0) } {}

template <BufferType BT>
Buffer<BT>& Buffer<BT>::operator=(Buffer&& other) noexcept {
	if (this != &other) {
		DeleteBuffer();
		id_	   = std::exchange(other.id_, 0);
		count_ = std::exchange(other.count_, 0);
	}
	return *this;
}

template <BufferType BT>
void Buffer<BT>::SetSubData(
	const void* data, std::int32_t byte_offset, std::uint32_t element_count,
	std::uint32_t element_size, bool unbind_vertex_array
) {
	PTGN_ASSERT(element_count > 0, "Number of buffer elements must be greater than 0");
	PTGN_ASSERT(element_size > 0, "Byte size of a buffer element must be greater than 0");

	PTGN_ASSERT(data != nullptr);

	if (unbind_vertex_array) {
		// Ensure that this buffer does not get bound to any currently bound vertex array.
		VertexArray::Unbind();
	}

	Bind();

	std::uint32_t size{ element_count * element_size };
	// This buffer size check must be done after the buffer is bound.
	PTGN_ASSERT(size <= GetBoundSize(), "Attempting to bind data outside of allocated buffer size");
	GLCall(gl::BufferSubData(static_cast<gl::GLenum>(BT), byte_offset, size, data));
}

template <BufferType BT>
std::uint32_t Buffer<BT>::GetElementCount() const {
	return count_;
}

template <BufferType BT>
std::uint32_t Buffer<BT>::GetBoundId() {
	std::int32_t id{ -1 };
	GLCall(gl::glGetIntegerv(static_cast<gl::GLenum>(impl::GetGLBinding<BT>()), &id));
	PTGN_ASSERT(id >= 0, "Failed to retrieve bound buffer id");
	return static_cast<std::uint32_t>(id);
}

template <BufferType BT>
std::uint32_t Buffer<BT>::GetBoundSize() {
	std::int32_t size{ -1 };
	GLCall(gl::GetBufferParameteriv(static_cast<gl::GLenum>(BT), GL_BUFFER_SIZE, &size));
	PTGN_ASSERT(size >= 0, "Could not determine bound buffer size correctly");
	return static_cast<std::uint32_t>(size);
}

template <BufferType BT>
BufferUsage Buffer<BT>::GetBoundUsage() {
	std::int32_t usage{ -1 };
	GLCall(gl::GetBufferParameteriv(static_cast<gl::GLenum>(BT), GL_BUFFER_SIZE, &usage));
	PTGN_ASSERT(usage >= 0, "Could not determine bound buffer usage correctly");
	return static_cast<BufferUsage>(usage);
}

template <BufferType BT>
void Buffer<BT>::Bind() const {
	PTGN_ASSERT(IsValid(), "Cannot bind destroyed or uninitialized buffer");
	Bind(id_);
}

template <BufferType BT>
bool Buffer<BT>::IsBound() const {
	return GetBoundId() == id_;
}

template <BufferType BT>
void Buffer<BT>::Bind(std::uint32_t id) {
	GLCall(gl::BindBuffer(static_cast<gl::GLenum>(BT), id));
#ifdef PTGN_DEBUG
	++game.stats.buffer_binds;
#endif
#ifdef GL_ANNOUNCE_BUFFER_CALLS
	PTGN_LOG("GL: Bound buffer with id ", id);
#endif
}

template <BufferType BT>
void Buffer<BT>::GenerateBuffer() {
	GLCall(gl::GenBuffers(1, &id_));
	PTGN_ASSERT(IsValid(), "Failed to generate buffer using OpenGL context");
#ifdef GL_ANNOUNCE_BUFFER_CALLS
	PTGN_LOG("GL: Generated buffer with id ", id_);
#endif
}

template <BufferType BT>
void Buffer<BT>::DeleteBuffer() noexcept {
	if (!IsValid()) {
		return;
	}
	GLCall(gl::DeleteBuffers(1, &id_));
#ifdef GL_ANNOUNCE_BUFFER_CALLS
	PTGN_LOG("GL: Deleted buffer with id ", id_);
#endif
	id_ = 0;
}

template <BufferType BT>
bool Buffer<BT>::IsValid() const {
	return id_;
}

template class Buffer<BufferType::Vertex>;
template class Buffer<BufferType::Index>;
template class Buffer<BufferType::Uniform>;

} // namespace ptgn::impl