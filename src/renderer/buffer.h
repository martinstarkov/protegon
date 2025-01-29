#pragma once

#include <cstdint>

#include "renderer/gl_types.h"

namespace ptgn::impl {

template <BufferType BT>
struct Buffer {
	Buffer() = default;

	// @param data Pointer to the buffer data.
	// @param element_count Number of buffer elements to allocate.
	// @param element_size Size of a single buffer element in bytes.
	Buffer(
		const void* data, std::uint32_t element_count, std::uint32_t element_size, BufferUsage usage
	);

	Buffer(const Buffer&)				 = delete;
	Buffer& operator=(const Buffer&)	 = delete;
	Buffer(Buffer&&) noexcept			 = default;
	Buffer& operator=(Buffer&&) noexcept = default;
	~Buffer();

	// @param data Pointer to the new buffer data.
	// @param byte_offset Specifies the offset into the buffer object's data store where data
	// replacement will begin, measured in bytes.
	// @param element_count Number of buffer elements to allocate.
	// @param element_size Size of a single buffer element in bytes.
	// @param unbind_vertex_array If true (default), unbinds the current vertex array before setting
	// new data. This ensures that no vertex array is accidentally modified.
	void SetSubData(
		const void* data, std::int32_t byte_offset, std::uint32_t element_count,
		std::uint32_t element_size, bool unbind_vertex_array
	);

	// @return Number of elements in the buffer.
	[[nodiscard]] std::uint32_t GetElementCount() const;

	[[nodiscard]] static std::int32_t GetBoundId();
	[[nodiscard]] static std::int32_t GetBoundSize();
	[[nodiscard]] static BufferUsage GetBoundUsage();

	void Bind() const;

	std::uint32_t id_{ 0 };
	std::uint32_t count_{ 0 }; // Max number of elements in the buffer.
};

using VertexBuffer	= Buffer<BufferType::Vertex>;
using IndexBuffer	= Buffer<BufferType::Index>;
using UniformBuffer = Buffer<BufferType::Uniform>;

} // namespace ptgn::impl