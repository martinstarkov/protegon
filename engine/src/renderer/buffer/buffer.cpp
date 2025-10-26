#include "renderer/buffer/buffer.h"

#include <cstdint>

#include "core/app/application.h"
#include "debug/core/debug_config.h"
#include "debug/runtime/assert.h"
#include "debug/runtime/debug_system.h"
#include "renderer/buffer/vertex_array.h"
#include "renderer/gl/gl_helper.h"
#include "renderer/gl/gl_loader.h"

namespace ptgn::impl {

template <BufferType BT>
Buffer<BT>::Buffer(
	const void* data, std::uint32_t element_count, std::uint32_t element_size, BufferUsage usage
) {
	PTGN_ASSERT(usage != BufferUsage::Unset);
	PTGN_ASSERT(element_count > 0, "Number of buffer elements must be greater than 0");
	PTGN_ASSERT(element_size > 0, "Byte size of a buffer element must be greater than 0");

	GenerateBuffer();

	usage_ = usage;
	count_ = element_count;
	// Ensure that this buffer does not get bound to any currently bound vertex array.
	VertexArray::Unbind();

	std::uint32_t size{ element_count * element_size };

	Bind();

	GLCall(BufferData(static_cast<GLenum>(BT), size, data, static_cast<GLenum>(usage)));
}

template <BufferType BT>
Buffer<BT>::~Buffer() {
	DeleteBuffer();
}

template <BufferType BT>
Buffer<BT>::Buffer(Buffer&& other) noexcept :
	id_{ std::exchange(other.id_, 0) },
	count_{ std::exchange(other.count_, 0) },
	usage_{ std::exchange(other.usage_, BufferUsage::Unset) } {}

template <BufferType BT>
Buffer<BT>& Buffer<BT>::operator=(Buffer&& other) noexcept {
	if (this != &other) {
		DeleteBuffer();
		id_	   = std::exchange(other.id_, 0);
		count_ = std::exchange(other.count_, 0);
		usage_ = std::exchange(other.usage_, BufferUsage::Unset);
	}
	return *this;
}

template <BufferType BT>
void Buffer<BT>::SetSubData(
	const void* data, std::int32_t byte_offset, std::uint32_t element_count,
	std::uint32_t element_size, bool unbind_vertex_array, bool buffer_orphaning
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

	if (buffer_orphaning &&
		(usage_ == BufferUsage::DynamicDraw || usage_ == BufferUsage::StreamDraw)) {
		std::uint32_t buffer_size{ count_ * element_size };
		PTGN_ASSERT(
			buffer_size <= GetBoundSize(), "Buffer element size does not appear to match the "
										   "originally allocated buffer element size"
		);
		GLCall(
			BufferData(static_cast<GLenum>(BT), buffer_size, nullptr, static_cast<GLenum>(usage_))
		);
	}

	GLCall(BufferSubData(static_cast<GLenum>(BT), byte_offset, size, data));
}

template <BufferType BT>
std::uint32_t Buffer<BT>::GetElementCount() const {
	return count_;
}

template <BufferType BT>
std::uint32_t Buffer<BT>::GetBoundId() {
	std::int32_t id{ -1 };
	GLCall(glGetIntegerv(static_cast<GLenum>(impl::GetGLBinding<BT>()), &id));
	PTGN_ASSERT(id >= 0, "Failed to retrieve bound buffer id");
	return static_cast<std::uint32_t>(id);
}

template <BufferType BT>
std::uint32_t Buffer<BT>::GetBoundSize() {
	std::int32_t size{ -1 };
	GLCall(GetBufferParameteriv(static_cast<GLenum>(BT), GL_BUFFER_SIZE, &size));
	PTGN_ASSERT(size >= 0, "Could not determine bound buffer size correctly");
	return static_cast<std::uint32_t>(size);
}

template <BufferType BT>
BufferUsage Buffer<BT>::GetBoundUsage() {
	std::int32_t usage{ -1 };
	GLCall(GetBufferParameteriv(static_cast<GLenum>(BT), GL_BUFFER_SIZE, &usage));
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
	GLCall(BindBuffer(static_cast<GLenum>(BT), id));
#ifdef PTGN_DEBUG
	++Application::Get().debug_.stats.buffer_binds;
#endif
#ifdef GL_ANNOUNCE_BUFFER_CALLS
	PTGN_LOG("GL: Bound buffer with id ", id);
#endif
}

template <BufferType BT>
void Buffer<BT>::GenerateBuffer() {
	GLCall(GenBuffers(1, &id_));
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
	GLCall(DeleteBuffers(1, &id_));
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