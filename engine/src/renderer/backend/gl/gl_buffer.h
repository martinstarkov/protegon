#pragma once

#include <cstdint>
#include <ostream>

#include "core/util/concepts.h"
#include "core/util/id_map.h"
#include "renderer/resources/id.h"

namespace ptgn::impl::gl {

class GLContext;

enum class BufferUsage : std::uint32_t {
	StaticDraw	= 0x88E4, // GL_STATIC_DRAW
	DynamicDraw = 0x88E8, // GL_DYNAMIC_DRAW
	StreamDraw	= 0x88E0, // GL_STREAM_DRAW

	StaticRead	= 0x88E5, // GL_STATIC_READ
	DynamicRead = 0x88E9, // GL_DYNAMIC_READ
	StreamRead	= 0x88E1, // GL_STREAM_READ

	StaticCopy	= 0x88E6, // GL_STATIC_COPY
	DynamicCopy = 0x88EA, // GL_DYNAMIC_COPY
	StreamCopy	= 0x88E2  // GL_STREAM_COPY
};

std::ostream& operator<<(std::ostream& os, BufferUsage usage);

enum class BufferTarget : std::uint32_t {
	ArrayBuffer				= 0x8892, // GL_ARRAY_BUFFER
	AtomicCounterBuffer		= 0x92C0, // GL_ATOMIC_COUNTER_BUFFER
	CopyReadBuffer			= 0x8F36, // GL_COPY_READ_BUFFER
	CopyWriteBuffer			= 0x8F37, // GL_COPY_WRITE_BUFFER
	DispatchIndirectBuffer	= 0x90EE, // GL_DISPATCH_INDIRECT_BUFFER
	DrawIndirectBuffer		= 0x8F3F, // GL_DRAW_INDIRECT_BUFFER
	ElementArrayBuffer		= 0x8893, // GL_ELEMENT_ARRAY_BUFFER
	PixelPackBuffer			= 0x88EB, // GL_PIXEL_PACK_BUFFER
	PixelUnpackBuffer		= 0x88EC, // GL_PIXEL_UNPACK_BUFFER
	QueryBuffer				= 0x9192, // GL_QUERY_BUFFER
	ShaderStorageBuffer		= 0x90D2, // GL_SHADER_STORAGE_BUFFER
	TextureBuffer			= 0x8C2A, // GL_TEXTURE_BUFFER
	TransformFeedbackBuffer = 0x8C8E, // GL_TRANSFORM_FEEDBACK_BUFFER
	UniformBuffer			= 0x8A11  // GL_UNIFORM_BUFFER
};

std::ostream& operator<<(std::ostream& os, BufferTarget target);

enum class BufferParameter : std::uint32_t {
	Access			 = 0x88BB, // GL_BUFFER_ACCESS
	AccessFlags		 = 0x911F, // GL_BUFFER_ACCESS_FLAGS
	ImmutableStorage = 0x821F, // GL_BUFFER_IMMUTABLE_STORAGE
	Mapped			 = 0x88BC, // GL_BUFFER_MAPPED
	MapLength		 = 0x9120, // GL_BUFFER_MAP_LENGTH
	MapOffset		 = 0x9121, // GL_BUFFER_MAP_OFFSET
	Size			 = 0x8764, // GL_BUFFER_SIZE
	StorageFlags	 = 0x8220, // GL_BUFFER_STORAGE_FLAGS
	Usage			 = 0x8765  // GL_BUFFER_USAGE
};

std::ostream& operator<<(std::ostream& os, BufferParameter parameter);

struct BufferCache {
	BufferUsage usage{ BufferUsage::StaticDraw };
	std::uint32_t count{ 0 };
};

template <typename T>
concept BufferType = IsAnyOf<T, VertexBufferId, ElementBufferId, UniformBufferId>;

class Buffers {
public:
	VertexBufferId CreateVertexBuffer(
		const void* data, std::uint32_t element_count, std::uint32_t element_size, BufferUsage usage
	);

	ElementBufferId CreateElementBuffer(
		const void* data, std::uint32_t element_count, std::uint32_t element_size, BufferUsage usage
	);

	UniformBufferId CreateUniformBuffer(const void* data, std::uint32_t size, BufferUsage usage);

	void DestroyVertexBuffer(VertexBufferId id);
	void DestroyElementBuffer(ElementBufferId id);
	void DestroyUniformBuffer(UniformBufferId id);

	/// @param target OpenGL buffer binding point.
	template <BufferType T, bool kBufferOrphaning = true>
	void SetBufferSubData(
		T id, BufferTarget target, const void* data, std::int32_t byte_offset,
		std::uint32_t element_count, std::uint32_t element_size
	) const;

private:
	friend class GLContext;

	explicit Buffers(GLContext& gl);
	~Buffers() noexcept					   = default;
	Buffers(const Buffers&)				   = delete;
	Buffers(Buffers&&) noexcept			   = delete;
	Buffers& operator=(const Buffers&)	   = delete;
	Buffers& operator=(Buffers&&) noexcept = delete;

	template <BufferType T>
	T CreateBuffer(
		BufferTarget target, const void* data, std::uint32_t element_count,
		std::uint32_t element_size, BufferUsage usage
	);

	template <BufferType T>
	void DestroyBuffer(T id);

	int GetBufferParameter(BufferTarget target, BufferParameter parameter) const;

	IdMap<BufferCache> cache_;

	GLContext& gl_;
};

} // namespace ptgn::impl::gl