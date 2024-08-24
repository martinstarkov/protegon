#include "protegon/buffer.h"

#include "protegon/vertex_array.h"
#include "renderer/gl_loader.h"

namespace ptgn {

namespace impl {

BufferInstance::BufferInstance() {
	gl::GenBuffers(1, &id_);
	PTGN_ASSERT(id_ != 0, "Failed to generate buffer using OpenGL context");
}

BufferInstance::~BufferInstance() {
	gl::DeleteBuffers(1, &id_);
}

// Returns the max buffer size (as set by glBufferData) of the currently bound buffer.
[[nodiscard]] inline static std::uint32_t GetMaxBufferSize(BufferType type) {
	std::int32_t max_size{ 0 };
	gl::GetBufferParameteriv(static_cast<gl::GLenum>(type), GL_BUFFER_SIZE, &max_size);
	return static_cast<std::uint32_t>(max_size);
}

} // namespace impl

template <BufferType BT>
void Buffer<BT>::SetDataImpl(const void* data, std::uint32_t size, BufferUsage usage) {
	PTGN_ASSERT(size != 0, "Must provide more than one element when creating buffer");
	PTGN_ASSERT(data != nullptr);
	VertexArray::Unbind();
	Bind();
	gl::BufferData(static_cast<gl::GLenum>(BT), size, data, static_cast<gl::GLenum>(usage));
}

template <BufferType BT>
void Buffer<BT>::SetSubData(const void* data, std::uint32_t size) {
	PTGN_ASSERT(size != 0, "Must provide more than one element when setting buffer subdata");
	PTGN_ASSERT(data != nullptr);
	VertexArray::Unbind();
	Bind();
	// assert check must be done after buffer is bound
	PTGN_ASSERT(size <= impl::GetMaxBufferSize(BT));
	gl::BufferSubData(static_cast<gl::GLenum>(BT), 0, size, data);
}

template <BufferType BT>
std::int32_t Buffer<BT>::BoundId() {
	std::int32_t id{ -1 };
	if constexpr (BT == BufferType::Vertex) {
		gl::glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &id);
	} else if constexpr (BT == BufferType::Index) {
		gl::glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &id);
	} else if constexpr (BT == BufferType::Uniform) {
		gl::glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &id);
	}
	PTGN_ASSERT(id >= 0, "Unrecognized type for bound id check");
	return id;
}

template <BufferType BT>
void Buffer<BT>::Bind() const {
	PTGN_ASSERT(IsValid(), "Cannot bind uninitialized or destroyed buffer");
	gl::BindBuffer(static_cast<gl::GLenum>(BT), instance_->id_);
}

// template <BufferType BT>
// void Buffer<BT>::Unbind() {
//	gl::BindBuffer(static_cast<gl::GLenum>(BT), 0);
// }

template class Buffer<BufferType::Vertex>;
template class Buffer<BufferType::Index>;
template class Buffer<BufferType::Uniform>;

} // namespace ptgn
