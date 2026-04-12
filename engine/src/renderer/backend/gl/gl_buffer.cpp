#include "renderer/backend/gl/gl_buffer.h"

#include <cstdint>
#include <ostream>
#include <utility>

#include "core/assert.h"
#include "core/log.h"
#include "core/util/id_map.h"
#include "renderer/backend/gl/gl.h"
#include "renderer/backend/gl/gl_context.h"
#include "renderer/primitives/id.h"

namespace ptgn::impl::gl {

Buffers::Buffers(GLContext& gl) : gl_{ gl } {}

VertexBufferId Buffers::CreateVertexBuffer(
	const void* data, std::uint32_t element_count, std::uint32_t element_size, BufferUsage usage
) {
	return CreateBuffer<VertexBufferId>(
		BufferTarget::ArrayBuffer, data, element_count, element_size, usage
	);
}

ElementBufferId Buffers::CreateElementBuffer(
	const void* data, std::uint32_t element_count, std::uint32_t element_size, BufferUsage usage
) {
	return CreateBuffer<ElementBufferId>(
		BufferTarget::ElementArrayBuffer, data, element_count, element_size, usage
	);
}

UniformBufferId Buffers::CreateUniformBuffer(
	const void* data, std::uint32_t size, BufferUsage usage
) {
	return CreateBuffer<UniformBufferId>(BufferTarget::UniformBuffer, data, size, 1, usage);
}

void Buffers::DestroyVertexBuffer(VertexBufferId id) {
	DestroyBuffer<VertexBufferId>(id);
}

void Buffers::DestroyElementBuffer(ElementBufferId id) {
	DestroyBuffer<ElementBufferId>(id);
}

void Buffers::DestroyUniformBuffer(UniformBufferId id) {
	DestroyBuffer<UniformBufferId>(id);
}

template <BufferType T, bool kBufferOrphaning>
void Buffers::SetBufferSubData(
	T id, BufferTarget target, const void* data, std::int32_t byte_offset,
	std::uint32_t element_count, std::uint32_t element_size
) const {
	PTGN_ASSERT(gl_.IsBound(id), "Buffer must be bound before setting its subdata");
	PTGN_ASSERT(element_count > 0, "Number of buffer elements must be greater than 0");
	PTGN_ASSERT(element_size > 0, "Byte size of a buffer element must be greater than 0");

	PTGN_ASSERT(data != nullptr);

	std::uint32_t size{ element_count * element_size };

	// This buffer size check must be done after the buffer is bound.
	PTGN_ASSERT(
		(size <= static_cast<std::uint32_t>(
					 GetBufferParameter(BufferTarget::ArrayBuffer, BufferParameter::Size)
				 )),
		"Attempting to bind data outside of allocated buffer size"
	);

	if constexpr (kBufferOrphaning) {
		const auto& cache{ cache_.Get(id) };

		if (cache.usage == BufferUsage::DynamicDraw || cache.usage == BufferUsage::StreamDraw) {
			std::uint32_t buffer_size{ cache.count * element_size };
			PTGN_ASSERT(buffer_size > 0);
			PTGN_ASSERT(
				(buffer_size <= static_cast<std::uint32_t>(GetBufferParameter(
									BufferTarget::ArrayBuffer, BufferParameter::Size
								))),
				"Buffer element size does not appear to match the "
				"originally allocated buffer element size"
			);
			GLCall(glBufferData(
				std::to_underlying(target), buffer_size, nullptr, std::to_underlying(cache.usage)
			));
		}
	}

	GLCall(glBufferSubData(std::to_underlying(target), byte_offset, size, data));
}

template void Buffers::SetBufferSubData<VertexBufferId>(
	VertexBufferId, BufferTarget, const void*, std::int32_t, std::uint32_t, std::uint32_t
) const;
template void Buffers::SetBufferSubData<ElementBufferId>(
	ElementBufferId, BufferTarget, const void*, std::int32_t, std::uint32_t, std::uint32_t
) const;
template void Buffers::SetBufferSubData<UniformBufferId>(
	UniformBufferId, BufferTarget, const void*, std::int32_t, std::uint32_t, std::uint32_t
) const;

template <BufferType T>
T Buffers::CreateBuffer(
	BufferTarget target, const void* data, std::uint32_t element_count, std::uint32_t element_size,
	BufferUsage usage
) {
	PTGN_ASSERT(element_count > 0, "Number of buffer elements must be greater than 0");
	PTGN_ASSERT(element_size > 0, "Byte size of a buffer element must be greater than 0");

	T id{ 0 };
	GLCall(glGenBuffers(1, &id.value));

	PTGN_ASSERT(id, "Failed to create buffer");

	auto _1 = gl_.Bind(VertexArrayId{ 0 }, true);
	auto _2 = gl_.Bind(id, false);

	const std::uint32_t size = element_count * element_size;

	GLCall(glBufferData(std::to_underlying(target), size, data, std::to_underlying(usage)));

	cache_.Add(id, BufferCache{ .usage = usage, .count = element_count });

	return id;
}

template VertexBufferId Buffers::CreateBuffer<VertexBufferId>(
	BufferTarget, const void*, std::uint32_t, std::uint32_t, BufferUsage
);
template ElementBufferId Buffers::CreateBuffer<ElementBufferId>(
	BufferTarget, const void*, std::uint32_t, std::uint32_t, BufferUsage
);
template UniformBufferId Buffers::CreateBuffer<UniformBufferId>(
	BufferTarget, const void*, std::uint32_t, std::uint32_t, BufferUsage
);

template <BufferType T>
void Buffers::DestroyBuffer(T id) {
	if (!id) {
		return;
	}
	GLCall(glDeleteBuffers(1, &id.value));
	cache_.Remove(id);
}

template void Buffers::DestroyBuffer<VertexBufferId>(VertexBufferId);
template void Buffers::DestroyBuffer<ElementBufferId>(ElementBufferId);
template void Buffers::DestroyBuffer<UniformBufferId>(UniformBufferId);

int Buffers::GetBufferParameter(BufferTarget target, BufferParameter parameter) const {
	int value{ -1 };
	GLCall(glGetBufferParameteriv(std::to_underlying(target), std::to_underlying(parameter), &value)
	);
	PTGN_ASSERT(value >= 0, "Failed to query buffer parameter");
	return value;
}

std::ostream& operator<<(std::ostream& os, BufferUsage usage) {
	switch (usage) {
		using enum BufferUsage;
		case StaticDraw:  return os << "StaticDraw";
		case DynamicDraw: return os << "DynamicDraw";
		case StreamDraw:  return os << "StreamDraw";
		case StaticRead:  return os << "StaticRead";
		case DynamicRead: return os << "DynamicRead";
		case StreamRead:  return os << "StreamRead";
		case StaticCopy:  return os << "StaticCopy";
		case DynamicCopy: return os << "DynamicCopy";
		case StreamCopy:  return os << "StreamCopy";
		default:		  PTGN_ERROR("Unknown BufferUsage: ", std::to_underlying(usage));
	}
}

std::ostream& operator<<(std::ostream& os, BufferTarget target) {
	switch (target) {
		using enum BufferTarget;
		case ArrayBuffer:			  return os << "VertexBuffer";
		case AtomicCounterBuffer:	  return os << "AtomicCounterBuffer";
		case CopyReadBuffer:		  return os << "CopyReadBuffer";
		case CopyWriteBuffer:		  return os << "CopyWriteBuffer";
		case DispatchIndirectBuffer:  return os << "DispatchIndirectBuffer";
		case DrawIndirectBuffer:	  return os << "DrawIndirectBuffer";
		case ElementArrayBuffer:	  return os << "ElementBuffer";
		case PixelPackBuffer:		  return os << "PixelPackBuffer";
		case PixelUnpackBuffer:		  return os << "PixelUnpackBuffer";
		case QueryBuffer:			  return os << "QueryBuffer";
		case ShaderStorageBuffer:	  return os << "ShaderStorageBuffer";
		case TextureBuffer:			  return os << "TextureBuffer";
		case TransformFeedbackBuffer: return os << "TransformFeedbackBuffer";
		case UniformBuffer:			  return os << "UniformBuffer";
		default:					  PTGN_ERROR("Unknown BufferTarget: ", std::to_underlying(target));
	}
}

std::ostream& operator<<(std::ostream& os, BufferParameter parameter) {
	switch (parameter) {
		using enum BufferParameter;
		case Access:		   return os << "Access";
		case AccessFlags:	   return os << "AccessFlags";
		case ImmutableStorage: return os << "ImmutableStorage";
		case Mapped:		   return os << "Mapped";
		case MapLength:		   return os << "MapLength";
		case MapOffset:		   return os << "MapOffset";
		case Size:			   return os << "Size";
		case StorageFlags:	   return os << "StorageFlags";
		case Usage:			   return os << "Usage";
		default:			   PTGN_ERROR("Unknown BufferParameter: ", std::to_underlying(parameter));
	}
}

} // namespace ptgn::impl::gl