#include "renderer/backend/gl/gl_buffer.h"

#include <cstdint>
#include <utility>

#include "core/assert.h"
#include "renderer/backend/gl/gl.h"
#include "renderer/backend/gl/gl_context.h"

namespace ptgn::impl::gl {

Buffers::Buffers(GLContext& gl) : gl_{ gl } {}

VertexBuffer Buffers::CreateVertexBuffer(
	const void* data, std::uint32_t element_count, std::uint32_t element_size, BufferUsage usage
) {
	return CreateBuffer<VertexBuffer>(
		BufferTarget::ArrayBuffer, data, element_count, element_size, usage
	);
}

ElementBuffer Buffers::CreateElementBuffer(
	const void* data, std::uint32_t element_count, std::uint32_t element_size, BufferUsage usage
) {
	return CreateBuffer<ElementBuffer>(
		BufferTarget::ElementArrayBuffer, data, element_count, element_size, usage
	);
}

UniformBuffer Buffers::CreateUniformBuffer(
	const void* data, std::uint32_t size, BufferUsage usage
) {
	return CreateBuffer<UniformBuffer>(BufferTarget::UniformBuffer, data, size, 1, usage);
}

void Buffers::DestroyVertexBuffer(VertexBuffer id) {
	DestroyBuffer<VertexBuffer>(id);
}

void Buffers::DestroyElementBuffer(ElementBuffer id) {
	DestroyBuffer<ElementBuffer>(id);
}

void Buffers::DestroyUniformBuffer(UniformBuffer id) {
	DestroyBuffer<UniformBuffer>(id);
}

template <typename T, bool kBufferOrphaning>
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
			GLCall(BufferData(
				std::to_underlying(target), buffer_size, nullptr, std::to_underlying(cache.usage)
			));
		}
	}

	GLCall(BufferSubData(std::to_underlying(target), byte_offset, size, data));
}

template void Buffers::SetBufferSubData<VertexBuffer>(
	VertexBuffer, BufferTarget, const void*, std::int32_t, std::uint32_t, std::uint32_t
) const;
template void Buffers::SetBufferSubData<ElementBuffer>(
	ElementBuffer, BufferTarget, const void*, std::int32_t, std::uint32_t, std::uint32_t
) const;
template void Buffers::SetBufferSubData<UniformBuffer>(
	UniformBuffer, BufferTarget, const void*, std::int32_t, std::uint32_t, std::uint32_t
) const;

template <typename T>
T Buffers::CreateBuffer(
	BufferTarget target, const void* data, std::uint32_t element_count, std::uint32_t element_size,
	BufferUsage usage
) {
	PTGN_ASSERT(element_count > 0, "Number of buffer elements must be greater than 0");
	PTGN_ASSERT(element_size > 0, "Byte size of a buffer element must be greater than 0");

	T id{ 0 };
	GLCall(GenBuffers(1, &id));

	PTGN_ASSERT(id, "Failed to create buffer");

	auto _1 = gl_.Bind(VertexArray{ 0 }, true);
	auto _2 = gl_.Bind(id, false);

	const std::uint32_t size = element_count * element_size;

	GLCall(BufferData(std::to_underlying(target), size, data, std::to_underlying(usage)));

	cache_.Add(id, BufferCache{ .usage = usage, .count = element_count });

	return id;
}

template VertexBuffer Buffers::CreateBuffer<VertexBuffer>(
	BufferTarget, const void*, std::uint32_t, std::uint32_t, BufferUsage
);
template ElementBuffer Buffers::CreateBuffer<ElementBuffer>(
	BufferTarget, const void*, std::uint32_t, std::uint32_t, BufferUsage
);
template UniformBuffer Buffers::CreateBuffer<UniformBuffer>(
	BufferTarget, const void*, std::uint32_t, std::uint32_t, BufferUsage
);

template <typename T>
void DestroyBuffer(T id) {
	if (!id) {
		return;
	}
	GLCall(DeleteBuffers(1, &id));
	cache_.Remove(id);
}

template void Buffers::DestroyBuffer<VertexBuffer>(VertexBuffer);
template void Buffers::DestroyBuffer<ElementBuffer>(ElementBuffer);
template void Buffers::DestroyBuffer<UniformBuffer>(UniformBuffer);

int Buffers::GetBufferParameter(BufferTarget target, BufferParameter parameter) const {
	int value{ -1 };
	GLCall(GetBufferParameteriv(std::to_underlying(target), std::to_underlying(parameter), &value));
	PTGN_ASSERT(value >= 0, "Failed to query buffer parameter");
	return value;
}

} // namespace ptgn::impl::gl